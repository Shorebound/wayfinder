#include "app/PerformanceOverlay.h"

#include "app/EngineContext.h"

#include <doctest/doctest.h>

#include <cmath>

namespace Wayfinder::Tests
{
    TEST_SUITE("PerformanceOverlay")
    {
        TEST_CASE("PerformanceOverlay attaches and detaches cleanly")
        {
            PerformanceOverlay overlay;
            EngineContext ctx;

            auto attachResult = overlay.OnAttach(ctx);
            CHECK(attachResult.has_value());

            auto detachResult = overlay.OnDetach(ctx);
            CHECK(detachResult.has_value());
        }

        TEST_CASE("PerformanceOverlay returns correct name")
        {
            PerformanceOverlay overlay;
            CHECK(overlay.GetName() == "PerformanceOverlay");
        }

        TEST_CASE("PerformanceOverlay averaging produces correct values")
        {
            PerformanceOverlay overlay;
            EngineContext ctx;
            overlay.OnAttach(ctx);

            // Simulate 10 frames at ~60 FPS (16.67ms per frame).
            constexpr float DELTA = 0.01667f;
            for (int i = 0; i < 20; ++i)
            {
                overlay.OnUpdate(ctx, DELTA);
            }

            // After enough time has passed (20 * 16.67ms = 333ms > REFRESH_INTERVAL of 250ms),
            // display values should have been computed at least once.
            float displayFps = overlay.GetDisplayFps();
            float displayMs = overlay.GetDisplayMs();

            // Allow reasonable tolerance for floating point.
            CHECK(displayFps > 50.0f);
            CHECK(displayFps < 70.0f);
            CHECK(displayMs > 14.0f);
            CHECK(displayMs < 20.0f);
        }

        TEST_CASE("PerformanceOverlay OnRender is safe without ImGui context")
        {
            PerformanceOverlay overlay;
            EngineContext ctx;

            // Should not crash in headless mode.
            overlay.OnRender(ctx);
        }

        TEST_CASE("PerformanceOverlay OnUpdate resets accumulators after refresh")
        {
            PerformanceOverlay overlay;
            EngineContext ctx;
            overlay.OnAttach(ctx);

            // Feed enough frames to trigger at least one refresh cycle.
            constexpr float DELTA = 0.01667f;
            for (int i = 0; i < 20; ++i)
            {
                overlay.OnUpdate(ctx, DELTA);
            }

            // After refresh, we should have valid display values.
            float fps1 = overlay.GetDisplayFps();
            CHECK(fps1 > 0.0f);

            // Feed another batch of frames at a different rate (~30 FPS).
            constexpr float SLOW_DELTA = 0.03333f;
            for (int i = 0; i < 10; ++i)
            {
                overlay.OnUpdate(ctx, SLOW_DELTA);
            }

            float fps2 = overlay.GetDisplayFps();
            // Second batch at ~30 FPS should produce different display values.
            CHECK(fps2 > 25.0f);
            CHECK(fps2 < 40.0f);
        }
    }

} // namespace Wayfinder::Tests
