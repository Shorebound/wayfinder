#include "ProjectDescriptor.h"
#include "core/Log.h"

#include <system_error>
#include <toml++/toml.hpp>

namespace Wayfinder
{

    std::filesystem::path ProjectDescriptor::ResolveModulePath() const
    {
        if (Paths.Module.empty())
        {
            return {};
        }

#ifdef _WIN32
        return ProjectRoot / (Paths.Module + ".dll");
#elif defined(__APPLE__)
        return ProjectRoot / ("lib" + Paths.Module + ".dylib");
#else
        return ProjectRoot / ("lib" + Paths.Module + ".so");
#endif
    }

    Result<ProjectLoadOutput> ProjectDescriptor::LoadFromFile(const std::filesystem::path& path)
    {
        ProjectLoadOutput output{};

        {
            std::error_code ec;
            const bool pathExists = std::filesystem::exists(path, ec);
            if (ec || !pathExists)
            {
                WAYFINDER_ERROR(LogEngine, "Project file not found: {}", path.string());
                return MakeError("Project file not found");
            }
        }

        {
            std::error_code ec;
            const auto canonicalPath = std::filesystem::weakly_canonical(path, ec);
            if (ec)
            {
                WAYFINDER_ERROR(LogEngine, "Failed to resolve project root for {}: {}", path.string(), ec.message());
                return MakeError("Failed to resolve project root");
            }
            output.Descriptor.ProjectRoot = canonicalPath.parent_path();
        }

        try
        {
            const toml::table tbl = toml::parse_file(path.string());

            if (const auto* project = tbl["project"].as_table())
            {
                if (auto v = (*project)["name"].value<std::string>())
                {
                    output.Descriptor.Name = *v;
                }
                if (auto v = (*project)["version"].value<std::string>())
                {
                    output.Descriptor.Version = *v;
                }
                if (auto v = (*project)["engine_version"].value<std::string>())
                {
                    output.Descriptor.EngineVersion = *v;
                }
                if (auto v = (*project)["module"].value<std::string>())
                {
                    output.Descriptor.Paths.Module = *v;
                }
            }

            if (const auto* paths = tbl["paths"].as_table())
            {
                if (auto v = (*paths)["asset_root"].value<std::string>())
                {
                    output.Descriptor.Paths.AssetRoot = *v;
                }
                if (auto v = (*paths)["boot_scene"].value<std::string>())
                {
                    output.Descriptor.Paths.BootScene = *v;
                }
                if (auto v = (*paths)["config_dir"].value<std::string>())
                {
                    output.Descriptor.Paths.ConfigDir = *v;
                }
            }

            WAYFINDER_INFO(LogEngine, "Loaded project '{}' v{} from: {}", output.Descriptor.Name, output.Descriptor.Version, path.string());
        }
        catch (const toml::parse_error& err)
        {
            WAYFINDER_ERROR(LogEngine, "Failed to parse project file {}: {}", path.string(), err.what());
            return MakeError("Failed to parse project file");
        }

        // --- Post-parse validation ---
        if (output.Descriptor.Name.empty() || output.Descriptor.Name == DEFAULT_PROJECT_NAME)
        {
            output.Warnings.emplace_back("Project name is missing or empty — [project] table may be incomplete");
        }

        if (output.Descriptor.Paths.BootScene.empty())
        {
            output.Warnings.emplace_back("Boot scene path is empty — the engine may fail to start");
        }

        return output;
    }

} // namespace Wayfinder
