#include <doctest/doctest.h>

#include "core/BackendConfig.h"
#include "core/EngineConfig.h"
#include "core/EngineContext.h"
#include "core/EngineRuntime.h"
#include "core/ProjectDescriptor.h"
#include "platform/Input.h"
#include "platform/Time.h"
#include "platform/Window.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/pipeline/Renderer.h"

using namespace Wayfinder;

namespace Wayfinder::Tests
{
    /// Constructs a headless-friendly EngineConfig with null backends.
    EngineConfig MakeNullConfig()
    {
        EngineConfig cfg{};
        cfg.Backends.Platform = PlatformBackend::Null;
        cfg.Backends.Rendering = RenderBackend::Null;
        cfg.Window.Width = 320;
        cfg.Window.Height = 240;
        cfg.Window.Title = "Test";
        return cfg;
    }

    /// Constructs a minimal ProjectDescriptor with no filesystem expectations.
    ProjectDescriptor MakeTestProject()
    {
        ProjectDescriptor desc{};
        desc.Name = "TestProject";
        return desc;
    }
} // namespace

TEST_SUITE("EngineRuntime")
{
    TEST_CASE("EngineRuntime initialises with null backends")
    {
        auto config = MakeNullConfig();
        auto project = MakeTestProject();

        EngineRuntime runtime(config, project);
        CHECK(runtime.Initialise());
        runtime.Shutdown();
    }

    TEST_CASE("EngineRuntime is constructible without Application")
    {
        auto config = MakeNullConfig();
        auto project = MakeTestProject();

        EngineRuntime runtime(config, project);
        REQUIRE(runtime.Initialise());

        // Accessors return valid references after initialisation
        CHECK_NOTHROW(runtime.GetWindow());
        CHECK_NOTHROW(runtime.GetInput());
        CHECK_NOTHROW(runtime.GetTime());
        CHECK_NOTHROW(runtime.GetDevice());
        CHECK_NOTHROW(runtime.GetRenderer());

        runtime.Shutdown();
    }

    TEST_CASE("BeginFrame / EndFrame lifecycle runs without crash")
    {
        auto config = MakeNullConfig();
        auto project = MakeTestProject();

        EngineRuntime runtime(config, project);
        REQUIRE(runtime.Initialise());

        // Simulate a few frames
        for (int i = 0; i < 3; ++i)
        {
            CHECK_NOTHROW(runtime.BeginFrame());
            CHECK_NOTHROW(runtime.EndFrame());
        }

        runtime.Shutdown();
    }

    TEST_CASE("GetDeltaTime returns non-negative after BeginFrame")
    {
        auto config = MakeNullConfig();
        auto project = MakeTestProject();

        EngineRuntime runtime(config, project);
        REQUIRE(runtime.Initialise());

        runtime.BeginFrame();
        CHECK(runtime.GetDeltaTime() >= 0.0f);
        runtime.EndFrame();

        runtime.Shutdown();
    }

    TEST_CASE("ShouldClose is false after initialisation")
    {
        auto config = MakeNullConfig();
        auto project = MakeTestProject();

        EngineRuntime runtime(config, project);
        REQUIRE(runtime.Initialise());

        CHECK_FALSE(runtime.ShouldClose());

        runtime.Shutdown();
    }

    TEST_CASE("Shutdown is safe to call twice")
    {
        auto config = MakeNullConfig();
        auto project = MakeTestProject();

        EngineRuntime runtime(config, project);
        REQUIRE(runtime.Initialise());

        runtime.Shutdown();
        CHECK_NOTHROW(runtime.Shutdown());
    }

    TEST_CASE("BuildContext returns valid references")
    {
        auto config = MakeNullConfig();
        auto project = MakeTestProject();

        EngineRuntime runtime(config, project);
        REQUIRE(runtime.Initialise());

        auto ctx = runtime.BuildContext();
        CHECK(ctx.config.Window.Width == 320);
        CHECK(ctx.project.Name == "TestProject");

        runtime.Shutdown();
    }
}
