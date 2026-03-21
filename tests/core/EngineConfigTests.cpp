#include "TestHelpers.h"
#include "core/EngineConfig.h"


#include <doctest/doctest.h>

#include <filesystem>

using namespace Wayfinder;
using TestHelpers::FixturesDir;

// ── EngineConfig Tests ──────────────────────────────────

namespace Wayfinder::Tests
{

    TEST_SUITE("EngineConfig")
    {
        TEST_CASE("LoadDefaults returns default values")
        {
            auto config = EngineConfig::LoadDefaults();

            CHECK(config.Window.Width == 1920);
            CHECK(config.Window.Height == 1080);
            CHECK(config.Window.Title == "Wayfinder Engine");
            CHECK(config.Window.VSync == false);
            CHECK(config.Backends.Platform == PlatformBackend::SDL3);
            CHECK(config.Backends.Rendering == RenderBackend::SDL_GPU);
            CHECK(config.Shaders.Directory == "assets/shaders");
            CHECK(config.Physics.FixedTimestep == doctest::Approx(1.0f / 60.0f));
        }

        TEST_CASE("LoadFromFile reads test config")
        {
            auto path = FixturesDir() / "test_engine_config.toml";
            auto config = EngineConfig::LoadFromFile(path);

            CHECK(config.Window.Width == 800);
            CHECK(config.Window.Height == 600);
            CHECK(config.Window.Title == "Test Window");
            CHECK(config.Window.VSync == true);
            CHECK(config.Backends.Platform == PlatformBackend::SDL3);
            CHECK(config.Backends.Rendering == RenderBackend::Null);
            CHECK(config.Shaders.Directory == "test/shaders");
            CHECK(config.Physics.FixedTimestep == doctest::Approx(0.00833333333333f));
        }

        TEST_CASE("LoadFromFile with missing file returns defaults")
        {
            auto config = EngineConfig::LoadFromFile("nonexistent_path/engine.toml");

            CHECK(config.Window.Width == 1920);
            CHECK(config.Window.Height == 1080);
            CHECK(config.Window.Title == "Wayfinder Engine");
            CHECK(config.Window.VSync == false);
            CHECK(config.Backends.Platform == PlatformBackend::SDL3);
            CHECK(config.Backends.Rendering == RenderBackend::SDL_GPU);
            CHECK(config.Shaders.Directory == "assets/shaders");
            CHECK(config.Physics.FixedTimestep == doctest::Approx(1.0f / 60.0f));
        }
    }
}