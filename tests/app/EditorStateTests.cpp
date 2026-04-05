#include "app/EditorState.h"

#include "app/EngineContext.h"

#include <doctest/doctest.h>

namespace Wayfinder::Tests
{
    TEST_SUITE("EditorState")
    {
        TEST_CASE("EditorState enters and exits cleanly")
        {
            EditorState state;
            EngineContext ctx;

            auto enterResult = state.OnEnter(ctx);
            CHECK(enterResult.has_value());

            auto exitResult = state.OnExit(ctx);
            CHECK(exitResult.has_value());
        }

        TEST_CASE("EditorState returns correct name")
        {
            EditorState state;
            CHECK(state.GetName() == "EditorState");
        }

        TEST_CASE("EditorState OnUpdate and OnRender are safe headless")
        {
            EditorState state;
            EngineContext ctx;

            // These should not crash in headless mode (no window/ImGui context).
            state.OnUpdate(ctx, 0.016f);
            state.OnRender(ctx);
        }

        TEST_CASE("EditorState background preferences default to None")
        {
            EditorState state;
            CHECK(state.GetBackgroundPreferences() == BackgroundMode::None);
        }
    }

} // namespace Wayfinder::Tests
