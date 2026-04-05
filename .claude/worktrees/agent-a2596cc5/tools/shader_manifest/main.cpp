/// Build-time tool: compiles .slang shaders and writes shader_manifest.json
/// with per-stage resource counts for Shipping builds where the runtime
/// Slang compiler is unavailable.
///
/// Usage:
///   wayfinder_shader_manifest --source-dir <dir> --output <path> [shader_stems...]
///
/// Each shader_stem (e.g. "unlit") is compiled for VSMain (vertex) and
/// PSMain (fragment). Resource counts come from Slang reflection (IMetadata).

#include "rendering/materials/SlangCompiler.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace
{
    struct Args
    {
        std::string SourceDir;
        std::string OutputPath;
        std::vector<std::string> ShaderStems;
    };

    auto ParseArgs(const std::span<char* const> args) -> std::optional<Args>
    {
        Args parsedArgs;
        for (size_t i = 1; i < args.size(); ++i)
        {
            const std::string arg = args[i];
            if (arg == "--source-dir" and i + 1 < args.size())
            {
                parsedArgs.SourceDir = args[++i];
            }
            else if (arg == "--output" and i + 1 < args.size())
            {
                parsedArgs.OutputPath = args[++i];
            }
            else if (not arg.starts_with("--"))
            {
                parsedArgs.ShaderStems.push_back(arg);
            }
            else
            {
                std::cerr << "Unknown option: " << arg << '\n';
                return std::nullopt;
            }
        }

        if (parsedArgs.SourceDir.empty() or parsedArgs.OutputPath.empty() or parsedArgs.ShaderStems.empty())
        {
            std::cerr << "Usage: wayfinder_shader_manifest --source-dir <dir> --output <path> [shader_stems...]\n";
            return std::nullopt;
        }
        return parsedArgs;
    }

    auto CountsToJson(const Wayfinder::ShaderResourceCounts& c) -> nlohmann::json
    {
        return {
            {"uniformBuffers", c.UniformBuffers},
            {"samplers", c.Samplers},
            {"storageTextures", c.StorageTextures},
            {"storageBuffers", c.StorageBuffers},
        };
    }

    auto Run(const std::span<char* const> args) -> int
    {
        const auto parsed = ParseArgs(args);
        if (not parsed)
        {
            return EXIT_FAILURE;
        }

        const auto& sourceDir = parsed->SourceDir;
        const auto& outputPath = parsed->OutputPath;
        const auto& shaderStems = parsed->ShaderStems;

        Wayfinder::SlangCompiler compiler;
        Wayfinder::SlangCompiler::InitDesc desc;
        desc.SourceDirectory = sourceDir;
        const auto initResult = compiler.Initialise(desc);
        if (not initResult.has_value())
        {
            std::cerr << "Failed to initialise SlangCompiler with source directory: " << sourceDir << ": " << initResult.error().GetMessage() << '\n';
            return EXIT_FAILURE;
        }

        nlohmann::ordered_json manifest = nlohmann::ordered_json::object();

        bool hadError = false;
        for (const auto& stem : shaderStems)
        {
            nlohmann::ordered_json shaderEntry = nlohmann::ordered_json::object();

            auto vertResult = compiler.Compile(stem, "VSMain", Wayfinder::ShaderStage::Vertex);
            if (not vertResult.has_value())
            {
                std::cerr << "Failed to compile vertex stage for '" << stem << "': " << vertResult.error().GetMessage() << '\n';
                hadError = true;
                continue;
            }
            if (not vertResult->Resources)
            {
                std::cerr << "Failed to extract vertex reflection for '" << stem << "': compiler returned no reflection metadata\n";
                hadError = true;
                continue;
            }
            shaderEntry["vertex"] = CountsToJson(*vertResult->Resources);

            auto fragResult = compiler.Compile(stem, "PSMain", Wayfinder::ShaderStage::Fragment);
            if (not fragResult.has_value())
            {
                std::cerr << "Failed to compile fragment stage for '" << stem << "': " << fragResult.error().GetMessage() << '\n';
                hadError = true;
                continue;
            }
            if (not fragResult->Resources)
            {
                std::cerr << "Failed to extract fragment reflection for '" << stem << "': compiler returned no reflection metadata\n";
                hadError = true;
                continue;
            }
            shaderEntry["fragment"] = CountsToJson(*fragResult->Resources);

            manifest[stem] = std::move(shaderEntry);
        }

        if (hadError)
        {
            std::cerr << "Errors occurred during shader compilation.\n";
            return EXIT_FAILURE;
        }

        const auto parentDir = std::filesystem::path(outputPath).parent_path();
        if (not parentDir.empty())
        {
            std::error_code error;
            std::filesystem::create_directories(parentDir, error);
            if (error)
            {
                std::cerr << "Failed to create output directory '" << parentDir.string() << "': " << error.message() << '\n';
                return EXIT_FAILURE;
            }
        }

        std::ofstream out(outputPath);
        if (not out.is_open())
        {
            std::cerr << "Failed to open output file: " << outputPath << '\n';
            return EXIT_FAILURE;
        }

        out << manifest.dump(2) << '\n';
        out.flush();
        if (out.fail())
        {
            std::cerr << "Failed to write shader manifest: " << outputPath << '\n';
            return EXIT_FAILURE;
        }

        std::cout << "Wrote shader manifest: " << outputPath << " (" << manifest.size() << " shaders)\n";
        return EXIT_SUCCESS;
    }
} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape) - std::filesystem and iostream paths still trip this on CLI entry points.
int main(const int argc, char* argv[])
{
    try
    {
        return Run(std::span(argv, static_cast<size_t>(argc)));
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Unhandled exception: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unhandled unknown exception.\n";
        return EXIT_FAILURE;
    }
}
