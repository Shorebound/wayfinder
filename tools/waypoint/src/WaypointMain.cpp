#include "core/Log.h"
#include "assets/AssetRegistry.h"
#include "scene/Scene.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    void PrintUsage()
    {
        std::cout << "Usage:\n";
        std::cout << "  waypoint validate-assets <asset-root>\n";
        std::cout << "  waypoint validate <scene-path>\n";
        std::cout << "  waypoint roundtrip-save <scene-path> <output-path>\n";
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

    int RunValidate(const std::filesystem::path& scenePath)
    {
        Wayfinder::Scene scene{"Waypoint Validation Scene"};
        scene.Initialize();

        const bool success = scene.LoadFromFile(scenePath.string());
        scene.Shutdown();
        return success ? 0 : 1;
    }

    int RunRoundtripSave(const std::filesystem::path& scenePath, const std::filesystem::path& outputPath)
    {
        Wayfinder::Scene scene{"Waypoint Roundtrip Scene"};
        scene.Initialize();

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

    if (argc < 3)
    {
        PrintUsage();
        Wayfinder::Log::Shutdown();
        return 1;
    }

    const std::string command = argv[1];
    const std::filesystem::path scenePath = std::filesystem::path(argv[2]);

    int exitCode = 1;
    if (command == "validate")
    {
        exitCode = RunValidate(scenePath);
    }
    else if (command == "validate-assets")
    {
        exitCode = RunValidateAssets(scenePath);
    }
    else if (command == "roundtrip-save")
    {
        if (argc < 4)
        {
            PrintUsage();
            Wayfinder::Log::Shutdown();
            return 1;
        }

        exitCode = RunRoundtripSave(scenePath, std::filesystem::path(argv[3]));
    }
    else
    {
        PrintUsage();
    }

    Wayfinder::Log::Shutdown();
    return exitCode;
}