#include "ProjectDescriptor.h"
#include "Log.h"

#include <toml++/toml.hpp>

namespace Wayfinder
{

    ProjectDescriptor ProjectDescriptor::LoadFromFile(const std::filesystem::path& path)
    {
        ProjectDescriptor descriptor{};

        if (!std::filesystem::exists(path))
        {
            WAYFINDER_ERROR(LogEngine, "Project file not found: {}", path.string());
            return descriptor;
        }

        descriptor.ProjectRoot = std::filesystem::weakly_canonical(path).parent_path();

        try
        {
            const toml::table tbl = toml::parse_file(path.string());

            if (const auto* project = tbl["project"].as_table())
            {
                if (auto v = (*project)["name"].value<std::string>()) descriptor.Name = *v;
                if (auto v = (*project)["version"].value<std::string>()) descriptor.Version = *v;
                if (auto v = (*project)["engine_version"].value<std::string>()) descriptor.EngineVersion = *v;
            }

            if (const auto* paths = tbl["paths"].as_table())
            {
                if (auto v = (*paths)["asset_root"].value<std::string>()) descriptor.Paths.AssetRoot = *v;
                if (auto v = (*paths)["boot_scene"].value<std::string>()) descriptor.Paths.BootScene = *v;
                if (auto v = (*paths)["config_dir"].value<std::string>()) descriptor.Paths.ConfigDir = *v;
            }

            WAYFINDER_INFO(LogEngine, "Loaded project '{}' v{} from: {}",
                           descriptor.Name, descriptor.Version, path.string());
        }
        catch (const toml::parse_error& err)
        {
            WAYFINDER_ERROR(LogEngine, "Failed to parse project file {}: {}", path.string(), err.what());
        }

        return descriptor;
    }

} // namespace Wayfinder
