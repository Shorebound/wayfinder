#include "EngineConfig.h"
#include "Log.h"

#include <toml++/toml.hpp>

namespace Wayfinder
{

    namespace
    {
        PlatformBackend ParsePlatformBackend(std::string_view value)
        {
            if (value == "sdl3") return PlatformBackend::SDL3;
            WAYFINDER_WARNING(LogEngine, "Unknown platform backend '{}', defaulting to SDL3", value);
            return PlatformBackend::SDL3;
        }

        RenderBackend ParseRenderBackend(std::string_view value)
        {
            if (value == "sdl_gpu") return RenderBackend::SDL_GPU;
            if (value == "null") return RenderBackend::Null;
            WAYFINDER_WARNING(LogEngine, "Unknown render backend '{}', defaulting to SDL_GPU", value);
            return RenderBackend::SDL_GPU;
        }
    }

    EngineConfig EngineConfig::LoadDefaults()
    {
        return EngineConfig{};
    }

    EngineConfig EngineConfig::LoadFromFile(const std::filesystem::path& path)
    {
        EngineConfig config{};

        if (!std::filesystem::exists(path))
        {
            WAYFINDER_WARNING(LogEngine, "Config file not found: {}. Using defaults.", path.string());
            return config;
        }

        try
        {
            const toml::table tbl = toml::parse_file(path.string());

            if (const auto* window = tbl["window"].as_table())
            {
                if (auto v = (*window)["width"].value<uint32_t>()) config.Window.Width = *v;
                if (auto v = (*window)["height"].value<uint32_t>()) config.Window.Height = *v;
                if (auto v = (*window)["title"].value<std::string>()) config.Window.Title = *v;
                if (auto v = (*window)["vsync"].value<bool>()) config.Window.VSync = *v;
            }

            if (const auto* backends = tbl["backends"].as_table())
            {
                if (auto v = (*backends)["platform"].value<std::string>())
                    config.Backends.Platform = ParsePlatformBackend(*v);
                if (auto v = (*backends)["rendering"].value<std::string>())
                    config.Backends.Rendering = ParseRenderBackend(*v);
            }

            if (const auto* shaders = tbl["shaders"].as_table())
            {
                if (auto v = (*shaders)["directory"].value<std::string>()) config.Shaders.Directory = *v;
            }

            if (const auto* project = tbl["project"].as_table())
            {
                if (auto v = (*project)["name"].value<std::string>()) config.Project.Name = *v;
                if (auto v = (*project)["asset_root"].value<std::string>()) config.Project.AssetRoot = *v;
                if (auto v = (*project)["boot_scene"].value<std::string>()) config.Project.BootScene = *v;
            }

            WAYFINDER_INFO(LogEngine, "Loaded config from: {}", path.string());
        }
        catch (const toml::parse_error& err)
        {
            WAYFINDER_ERROR(LogEngine, "Failed to parse config file {}: {}", path.string(), err.what());
        }

        return config;
    }

} // namespace Wayfinder
