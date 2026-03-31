#include "EngineConfig.h"
#include "core/Log.h"

#include <toml++/toml.hpp>

namespace Wayfinder
{

    namespace
    {
        PlatformBackend ParsePlatformBackend(std::string_view value)
        {
            if (value == "sdl3")
            {
                return PlatformBackend::SDL3;
            }
            Log::Warn(LogEngine, "Unknown platform backend '{}', defaulting to SDL3", value);
            return PlatformBackend::SDL3;
        }

        RenderBackend ParseRenderBackend(std::string_view value)
        {
            if (value == "sdl_gpu")
            {
                return RenderBackend::SDL_GPU;
            }
            if (value == "null")
            {
                return RenderBackend::Null;
            }
            Log::Warn(LogEngine, "Unknown render backend '{}', defaulting to SDL_GPU", value);
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
            Log::Warn(LogEngine, "Config file not found: {}. Using defaults.", path.string());
            return config;
        }

        try
        {
            const toml::table tbl = toml::parse_file(path.string());

            if (const auto* window = tbl.get_as<toml::table>("window"))
            {
                if (const auto* width = window->get("width"); width != nullptr)
                {
                    if (auto v = width->value<uint32_t>())
                    {
                        config.Window.Width = *v;
                    }
                }
                if (const auto* height = window->get("height"); height != nullptr)
                {
                    if (auto v = height->value<uint32_t>())
                    {
                        config.Window.Height = *v;
                    }
                }
                if (const auto* title = window->get("title"); title != nullptr)
                {
                    if (auto v = title->value<std::string>())
                    {
                        config.Window.Title = *v;
                    }
                }
                if (const auto* vSync = window->get("vsync"); vSync != nullptr)
                {
                    if (auto v = vSync->value<bool>())
                    {
                        config.Window.VSync = *v;
                    }
                }
            }

            if (const auto* backends = tbl.get_as<toml::table>("backends"))
            {
                if (const auto* platform = backends->get("platform"); platform != nullptr)
                {
                    if (auto v = platform->value<std::string>())
                    {
                        config.Backends.Platform = ParsePlatformBackend(*v);
                    }
                }
                if (const auto* rendering = backends->get("rendering"); rendering != nullptr)
                {
                    if (auto v = rendering->value<std::string>())
                    {
                        config.Backends.Rendering = ParseRenderBackend(*v);
                    }
                }
            }

            if (const auto* shaders = tbl.get_as<toml::table>("shaders"))
            {
                if (const auto* directory = shaders->get("directory"); directory != nullptr)
                {
                    if (auto v = directory->value<std::string>())
                    {
                        config.Shaders.Directory = *v;
                    }
                }
                if (const auto* sourceDir = shaders->get("source_directory"); sourceDir != nullptr)
                {
                    if (auto v = sourceDir->value<std::string>())
                    {
                        config.Shaders.SourceDirectory = *v;
                    }
                    else
                    {
                        Log::Warn(LogEngine, "shaders.source_directory: expected a string, got {}",
                            sourceDir->type() == toml::node_type::none             ? "unknown"
                            : sourceDir->type() == toml::node_type::integer        ? "integer"
                            : sourceDir->type() == toml::node_type::floating_point ? "float"
                            : sourceDir->type() == toml::node_type::boolean        ? "boolean"
                            : sourceDir->type() == toml::node_type::array          ? "array"
                            : sourceDir->type() == toml::node_type::table          ? "table"
                                                                                   : "non-string");
                    }
                }
            }

            if (const auto* physics = tbl.get_as<toml::table>("physics"))
            {
                if (const auto* fixedTimestep = physics->get("fixed_timestep"); fixedTimestep != nullptr)
                {
                    if (auto v = fixedTimestep->value<double>())
                    {
                        config.Physics.FixedTimestep = static_cast<float>(*v);
                    }
                }
            }

            Log::Info(LogEngine, "Loaded config from: {}", path.string());
        }
        catch (const toml::parse_error& err)
        {
            Log::Error(LogEngine, "Failed to parse config file {}: {}", path.string(), err.what());
        }

        return config;
    }

} // namespace Wayfinder
