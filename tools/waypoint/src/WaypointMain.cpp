#include "app/EngineConfig.h"
#include "assets/AssetRegistry.h"
#include "core/Assert.h"
#include "core/Log.h"
#include "core/Result.h"
#include "plugins/PluginLoader.h"
#include "plugins/PluginRegistry.h"
#include "project/ProjectDescriptor.h"
#include "project/ProjectResolver.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"
#include "scene/SceneWorldBootstrap.h"

#include "ecs/Flecs.h"
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace
{
    void PrintUsage()
    {
        std::cout << "Usage:\n";
        std::cout << "  waypoint validate-assets <asset-root>\n";
        std::cout << "  waypoint validate <scene-path>\n";
        std::cout << "  waypoint roundtrip-save <scene-path> <output-path>\n";
        std::cout << "  waypoint --project [<path>]  validate-assets\n";
        std::cout << "  waypoint --project [<path>]  validate <scene-path>\n";
        std::cout << "  waypoint --project [<path>]  roundtrip-save <scene-path> <output-path>\n";
    }

    int RunValidateAssets(const std::filesystem::path& assetRoot)
    {
        Wayfinder::AssetRegistry assetRegistry;
        std::string error;
        if (!assetRegistry.BuildFromDirectory(assetRoot, error))
        {
            std::cerr << error << '\n';
            return 1;
        }

        return 0;
    }

    struct WaypointContext
    {
        flecs::world World;
        Wayfinder::RuntimeComponentRegistry Registry;
        std::optional<Wayfinder::Plugins::LoadedPlugin> GamePlugin;
        std::unique_ptr<Wayfinder::Plugins::PluginRegistry> PluginReg;
        /// Owned config; \ref PluginRegistry stores a reference — must outlive \ref PluginReg.
        Wayfinder::EngineConfig EngineConfig;

        explicit WaypointContext(const Wayfinder::ProjectDescriptor* project = nullptr, const std::filesystem::path& toolDir = {})
        {
            if (project)
            {
                EngineConfig = Wayfinder::EngineConfig::LoadFromFile(project->ResolveEngineConfigPath());
            }
            else
            {
                EngineConfig = Wayfinder::EngineConfig::LoadDefaults();
            }

            Wayfinder::Scene::RegisterCoreComponents(World);
            Registry.AddCoreEntries();

            if (project && !project->Paths.Plugin.empty())
            {
                auto pluginLibraryPath = project->ResolvePluginLibraryPath();

                /// If the plugin library isn't next to the project file, try the
                /// tool's own directory (common for build-output layouts).
                if (!std::filesystem::exists(pluginLibraryPath) && !toolDir.empty())
                {
                    pluginLibraryPath = toolDir / pluginLibraryPath.filename();
                }

                auto loadResult = Wayfinder::Plugins::PluginLoader::Load(pluginLibraryPath);
                if (loadResult && loadResult->Instance)
                {
                    GamePlugin = std::move(*loadResult);
                    PluginReg = std::make_unique<Wayfinder::Plugins::PluginRegistry>(*project, EngineConfig);
                    GamePlugin->Instance->Build(*PluginReg);
                    Registry.AddGameEntries(*PluginReg);
                }
                else
                {
                    std::cerr << "Warning: failed to load game plugin from " << pluginLibraryPath.string() << '\n';
                }
            }

            Registry.RegisterComponents(World);

            /// Mirror Game::InitialiseWorld: apply the plugin registry's systems and globals after
            /// component registration. Without a loaded game plugin, register the same core
            /// scene plugins (transform/camera) used by headless tests — do not combine both paths
            /// or systems would be registered twice.
            if (PluginReg)
            {
                PluginReg->ApplyToWorld(World);
            }
            else
            {
                Wayfinder::SceneWorldBootstrap::RegisterDefaultScenePlugins(World);
            }
        }
    };

    int RunValidate(const std::filesystem::path& scenePath, const Wayfinder::ProjectDescriptor* project = nullptr, const std::filesystem::path& toolDir = {})
    {
        WaypointContext ctx(project, toolDir);
        Wayfinder::Scene scene{ctx.World, ctx.Registry, "Waypoint Validation Scene"};

        const auto result = scene.LoadFromFile(scenePath.string());
        scene.Shutdown();
        return result ? 0 : 1;
    }

    int RunRoundtripSave(const std::filesystem::path& scenePath, const std::filesystem::path& outputPath, const Wayfinder::ProjectDescriptor* project = nullptr, const std::filesystem::path& toolDir = {})
    {
        WaypointContext ctx(project, toolDir);
        Wayfinder::Scene scene{ctx.World, ctx.Registry, "Waypoint Roundtrip Scene"};

        if (auto loadResult = scene.LoadFromFile(scenePath.string()); !loadResult)
        {
            scene.Shutdown();
            return 1;
        }

        const auto saveResult = scene.SaveToFile(outputPath.string());
        scene.Shutdown();
        return saveResult ? 0 : 1;
    }
} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape) — iostream may throw; acceptable for CLI entry.
int main(int argc, char** argv)
{
    Wayfinder::Log::Init();

    const auto toolDir = std::filesystem::path(argv[0]).parent_path();

    if (argc < 2)
    {
        PrintUsage();
        Wayfinder::Log::Shutdown();
        return 1;
    }

    // Parse optional --project flag
    int argIndex = 1;
    std::optional<Wayfinder::ProjectDescriptor> project;

    if (std::string(argv[argIndex]) == "--project")
    {
        ++argIndex;
        std::filesystem::path startPath = std::filesystem::current_path();

        // If the next arg exists and isn't a known command, treat it as a path
        if (argIndex < argc && argv[argIndex][0] != '-' && std::string(argv[argIndex]) != "validate" && std::string(argv[argIndex]) != "validate-assets" && std::string(argv[argIndex]) != "roundtrip-save")
        {
            startPath = std::filesystem::path(argv[argIndex]);
            ++argIndex;
        }

        const auto projectFile = Wayfinder::FindProjectFile(startPath);
        if (!projectFile)
        {
            std::cerr << "No project.wayfinder found from: " << startPath.string() << " (" << projectFile.error().GetMessage() << ")\n";
            Wayfinder::Log::Shutdown();
            return 1;
        }

        auto loadResult = Wayfinder::ProjectDescriptor::LoadFromFile(*projectFile);

        if (!loadResult)
        {
            std::cerr << "Failed to load project descriptor from: " << projectFile->string() << " (" << loadResult.error().GetMessage() << ")\n";
            Wayfinder::Log::Shutdown();
            return 1;
        }

        for (const auto& warning : loadResult->Warnings)
        {
            WAYFINDER_WARNING(Wayfinder::LogEngine, "Project: {}", warning);
        }

        project = std::move(loadResult->Descriptor);
    }

    if (argIndex >= argc)
    {
        PrintUsage();
        Wayfinder::Log::Shutdown();
        return 1;
    }

    const std::string command = argv[argIndex];
    ++argIndex;

    int exitCode = 1;
    if (command == "validate")
    {
        if (argIndex >= argc && !project)
        {
            PrintUsage();
            Wayfinder::Log::Shutdown();
            return 1;
        }

        std::filesystem::path scenePath;
        if (argIndex < argc)
        {
            scenePath = std::filesystem::path(argv[argIndex]);
        }
        else
        {
            WAYFINDER_ASSERT(project.has_value(), "Boot scene path requires --project when no scene argument is given");
            scenePath = project->ResolveBootScene();
        }

        exitCode = RunValidate(scenePath, project ? &*project : nullptr, toolDir);
    }
    else if (command == "validate-assets")
    {
        std::filesystem::path assetRoot;
        if (project)
        {
            assetRoot = project->ResolveAssetRoot();
        }
        else if (argIndex < argc)
        {
            assetRoot = std::filesystem::path(argv[argIndex]);
        }
        else
        {
            PrintUsage();
            Wayfinder::Log::Shutdown();
            return 1;
        }

        exitCode = RunValidateAssets(assetRoot);
    }
    else if (command == "roundtrip-save")
    {
        if (argIndex + 1 >= argc)
        {
            PrintUsage();
            Wayfinder::Log::Shutdown();
            return 1;
        }

        exitCode = RunRoundtripSave(std::filesystem::path(argv[argIndex]), std::filesystem::path(argv[argIndex + 1]), project ? &*project : nullptr, toolDir);
    }
    else
    {
        PrintUsage();
    }

    Wayfinder::Log::Shutdown();
    return exitCode;
}
