#include "core/EngineConfig.h"
#include "core/Log.h"
#include "core/ModuleLoader.h"
#include "core/ModuleRegistry.h"
#include "core/ProjectDescriptor.h"
#include "core/ProjectResolver.h"
#include "core/Result.h"
#include "assets/AssetRegistry.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"

#include <flecs.h>
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
        std::optional<Wayfinder::LoadedModule> Module;
        std::unique_ptr<Wayfinder::ModuleRegistry> ModReg;

        explicit WaypointContext(const Wayfinder::ProjectDescriptor* project = nullptr,
                                const std::filesystem::path& toolDir = {})
        {
            Wayfinder::Scene::RegisterCoreECS(World);
            Registry.AddCoreEntries();

            if (project && !project->Paths.Module.empty())
            {
                auto modulePath = project->ResolveModulePath();

                /// If the module isn't next to the project file, try the
                /// tool's own directory (common for build-output layouts).
                if (!std::filesystem::exists(modulePath) && !toolDir.empty())
                    modulePath = toolDir / modulePath.filename();

                auto loadResult = Wayfinder::ModuleLoader::Load(modulePath);
                if (loadResult && loadResult->Instance)
                {
                    Module = std::move(*loadResult);
                    auto defaultConfig = Wayfinder::EngineConfig::LoadDefaults();
                    ModReg = std::make_unique<Wayfinder::ModuleRegistry>(*project, defaultConfig);
                    Module->Instance->Register(*ModReg);
                    Registry.AddGameEntries(*ModReg);
                }
                else
                {
                    std::cerr << "Warning: failed to load game module from "
                              << modulePath.string() << '\n';
                }
            }

            Registry.RegisterComponents(World);
        }
    };

    int RunValidate(const std::filesystem::path& scenePath,
                    const Wayfinder::ProjectDescriptor* project = nullptr,
                    const std::filesystem::path& toolDir = {})
    {
        WaypointContext ctx(project, toolDir);
        Wayfinder::Scene scene{ctx.World, ctx.Registry, "Waypoint Validation Scene"};

        const bool success = scene.LoadFromFile(scenePath.string());
        scene.Shutdown();
        return success ? 0 : 1;
    }

    int RunRoundtripSave(const std::filesystem::path& scenePath,
                         const std::filesystem::path& outputPath,
                         const Wayfinder::ProjectDescriptor* project = nullptr,
                         const std::filesystem::path& toolDir = {})
    {
        WaypointContext ctx(project, toolDir);
        Wayfinder::Scene scene{ctx.World, ctx.Registry, "Waypoint Roundtrip Scene"};

        const bool loaded = scene.LoadFromFile(scenePath.string());
        if (!loaded)
        {
            scene.Shutdown();
            return 1;
        }

        const bool saved = scene.SaveToFile(outputPath.string());
        scene.Shutdown();
        return saved ? 0 : 1;
    }
}

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
        if (argIndex < argc && argv[argIndex][0] != '-'
            && std::string(argv[argIndex]) != "validate"
            && std::string(argv[argIndex]) != "validate-assets"
            && std::string(argv[argIndex]) != "roundtrip-save")
        {
            startPath = std::filesystem::path(argv[argIndex]);
            ++argIndex;
        }

        const auto projectFile = Wayfinder::FindProjectFile(startPath);
        if (!projectFile)
        {
            std::cerr << "No project.wayfinder found from: " << startPath.string()
                      << " (" << projectFile.error().GetMessage() << ")\n";
            Wayfinder::Log::Shutdown();
            return 1;
        }

        auto loadResult = Wayfinder::ProjectDescriptor::LoadFromFile(*projectFile);

        if (!loadResult)
        {
            std::cerr << "Failed to load project descriptor from: " << projectFile->string()
                      << " (" << loadResult.error().GetMessage() << ")\n";
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

        const std::filesystem::path scenePath = (argIndex < argc)
            ? std::filesystem::path(argv[argIndex])
            : project->ResolveBootScene();

        exitCode = RunValidate(scenePath, project ? &*project : nullptr, toolDir);
    }
    else if (command == "validate-assets")
    {
        std::filesystem::path assetRoot;
        if (project)
            assetRoot = project->ResolveAssetRoot();
        else if (argIndex < argc)
            assetRoot = std::filesystem::path(argv[argIndex]);
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

        exitCode = RunRoundtripSave(std::filesystem::path(argv[argIndex]),
                                    std::filesystem::path(argv[argIndex + 1]),
                                    project ? &*project : nullptr, toolDir);
    }
    else
    {
        PrintUsage();
    }

    Wayfinder::Log::Shutdown();
    return exitCode;
}