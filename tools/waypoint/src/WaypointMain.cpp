#include "app/EngineConfig.h"
#include "assets/AssetRegistry.h"
#include "core/Log.h"
#include "core/Result.h"
#include "plugins/PluginLoader.h"
#include "plugins/PluginRegistry.h"
#include "project/ProjectDescriptor.h"
#include "project/ProjectResolver.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"

#include "ecs/Flecs.h"
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace Wayfinder
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
        std::optional<Wayfinder::LoadedPlugin> GamePlugin;
        std::unique_ptr<Wayfinder::PluginRegistry> PluginReg;

        explicit WaypointContext(const Wayfinder::ProjectDescriptor* project = nullptr, const std::filesystem::path& toolDir = {})
        {
            Wayfinder::Scene::RegisterCoreComponents(World);
            Registry.AddCoreEntries();

            if (project && !project->Paths.Module.empty())
            {
                auto modulePath = project->ResolveModulePath();

                /// If the module isn't next to the project file, try the
                /// tool's own directory (common for build-output layouts).
                if (!std::filesystem::exists(modulePath) && !toolDir.empty())
                {
                    modulePath = toolDir / modulePath.filename();
                }

                auto loadResult = Wayfinder::PluginLoader::Load(modulePath);
                if (loadResult && loadResult->Instance)
                {
                    GamePlugin = std::move(*loadResult);
                    auto defaultConfig = Wayfinder::EngineConfig::LoadDefaults();
                    PluginReg = std::make_unique<Wayfinder::PluginRegistry>(*project, defaultConfig);
                    GamePlugin->Instance->Build(*PluginReg);
                    Registry.AddGameEntries(*PluginReg);
                }
                else
                {
                    std::cerr << "Warning: failed to load game plugin from " << modulePath.string() << '\n';
                }
            }

            Registry.RegisterComponents(World);
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
}

int main(int argc, char** argv)
{
    Wayfinder::Log::Init();

    const auto toolDir = std::filesystem::path(argv[0]).parent_path();

    if (argc < 2)
    {
        Wayfinder::PrintUsage();
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
        Wayfinder::PrintUsage();
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
            Wayfinder::PrintUsage();
            Wayfinder::Log::Shutdown();
            return 1;
        }

        const std::filesystem::path scenePath = (argIndex < argc) ? std::filesystem::path(argv[argIndex]) : project->ResolveBootScene();

        exitCode = Wayfinder::RunValidate(scenePath, project ? &*project : nullptr, toolDir);
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
            Wayfinder::PrintUsage();
            Wayfinder::Log::Shutdown();
            return 1;
        }

        exitCode = Wayfinder::RunValidateAssets(assetRoot);
    }
    else if (command == "roundtrip-save")
    {
        if (argIndex + 1 >= argc)
        {
            Wayfinder::PrintUsage();
            Wayfinder::Log::Shutdown();
            return 1;
        }

        exitCode = Wayfinder::RunRoundtripSave(std::filesystem::path(argv[argIndex]), std::filesystem::path(argv[argIndex + 1]), project ? &*project : nullptr, toolDir);
    }
    else
    {
        Wayfinder::PrintUsage();
    }

    Wayfinder::Log::Shutdown();
    return exitCode;
}
