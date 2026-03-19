#include "core/Log.h"
#include "core/ProjectDescriptor.h"
#include "core/ProjectResolver.h"
#include "assets/AssetRegistry.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"

#include <flecs.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <optional>

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

        WaypointContext()
        {
            Wayfinder::Scene::RegisterCoreECS(World);
            Registry.AddCoreEntries();
            Registry.RegisterComponents(World);
        }
    };

    int RunValidate(const std::filesystem::path& scenePath)
    {
        WaypointContext ctx;
        Wayfinder::Scene scene{ctx.World, ctx.Registry, "Waypoint Validation Scene"};

        const bool success = scene.LoadFromFile(scenePath.string());
        scene.Shutdown();
        return success ? 0 : 1;
    }

    int RunRoundtripSave(const std::filesystem::path& scenePath, const std::filesystem::path& outputPath)
    {
        WaypointContext ctx;
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
            std::cerr << "No project.wayfinder found from: " << startPath.string() << '\n';
            Wayfinder::Log::Shutdown();
            return 1;
        }

        project = Wayfinder::ProjectDescriptor::LoadFromFile(*projectFile);
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

        exitCode = RunValidate(scenePath);
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
                                    std::filesystem::path(argv[argIndex + 1]));
    }
    else
    {
        PrintUsage();
    }

    Wayfinder::Log::Shutdown();
    return exitCode;
}