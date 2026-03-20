#include "ProjectDescriptor.h"
#include "Log.h"

#include <toml++/toml.hpp>
#include <system_error>

namespace Wayfinder
{

    std::filesystem::path ProjectDescriptor::ResolveModulePath() const
    {
        if (Paths.Module.empty())
            return {};

#ifdef _WIN32
        return ProjectRoot / (Paths.Module + ".dll");
#elif defined(__APPLE__)
        return ProjectRoot / ("lib" + Paths.Module + ".dylib");
#else
        return ProjectRoot / ("lib" + Paths.Module + ".so");
#endif
    }

    ProjectDescriptor::LoadResult ProjectDescriptor::LoadFromFile(const std::filesystem::path& path)
    {
        LoadResult result{};

        if (!std::filesystem::exists(path))
        {
            WAYFINDER_ERROR(LogEngine, "Project file not found: {}", path.string());
            result.Valid = false;
            return result;
        }

        std::error_code ec;
        const auto canonicalPath = std::filesystem::weakly_canonical(path, ec);
        if (ec)
        {
            WAYFINDER_ERROR(LogEngine, "Failed to resolve project root for {}: {}", path.string(), ec.message());
            result.Valid = false;
            return result;
        }
        result.Descriptor.ProjectRoot = canonicalPath.parent_path();

        try
        {
            const toml::table tbl = toml::parse_file(path.string());

            if (const auto* project = tbl["project"].as_table())
            {
                if (auto v = (*project)["name"].value<std::string>()) result.Descriptor.Name = *v;
                if (auto v = (*project)["version"].value<std::string>()) result.Descriptor.Version = *v;
                if (auto v = (*project)["engine_version"].value<std::string>()) result.Descriptor.EngineVersion = *v;
                if (auto v = (*project)["module"].value<std::string>()) result.Descriptor.Paths.Module = *v;
            }

            if (const auto* paths = tbl["paths"].as_table())
            {
                if (auto v = (*paths)["asset_root"].value<std::string>()) result.Descriptor.Paths.AssetRoot = *v;
                if (auto v = (*paths)["boot_scene"].value<std::string>()) result.Descriptor.Paths.BootScene = *v;
                if (auto v = (*paths)["config_dir"].value<std::string>()) result.Descriptor.Paths.ConfigDir = *v;
            }

            WAYFINDER_INFO(LogEngine, "Loaded project '{}' v{} from: {}",
                           result.Descriptor.Name, result.Descriptor.Version, path.string());
        }
        catch (const toml::parse_error& err)
        {
            WAYFINDER_ERROR(LogEngine, "Failed to parse project file {}: {}", path.string(), err.what());
            result.Valid = false;
            return result;
        }

        // --- Post-parse validation ---
        if (result.Descriptor.Name.empty() || result.Descriptor.Name == DEFAULT_PROJECT_NAME)
        {
            result.Warnings.emplace_back("Project name is missing or empty — [project] table may be incomplete");
        }

        if (result.Descriptor.Paths.BootScene.empty())
        {
            result.Warnings.emplace_back("Boot scene path is empty — the engine may fail to start");
        }

        return result;
    }

} // namespace Wayfinder
