#pragma once

#include "platform/Input.h"

namespace Wayfinder
{
    /// Upper bounds matching SDL3Input for consistent out-of-range behaviour.
    inline constexpr KeyCode kNullMaxScancodes = 512;
    inline constexpr MouseCode kNullMaxMouseButtons = 8;

    class NullInput final : public Input
    {
    public:
        void BeginFrame() override {}

        bool IsKeyPressed(KeyCode key) const override { return key < kNullMaxScancodes ? false : false; }
        bool IsKeyDown(KeyCode key) const override { return key < kNullMaxScancodes ? false : false; }
        bool IsKeyReleased(KeyCode key) const override { return key < kNullMaxScancodes ? false : false; }
        bool IsKeyUp(KeyCode key) const override { return key < kNullMaxScancodes ? true : false; }

        bool IsMouseButtonPressed(MouseCode btn) const override { return btn < kNullMaxMouseButtons ? false : false; }
        bool IsMouseButtonDown(MouseCode btn) const override { return btn < kNullMaxMouseButtons ? false : false; }
        bool IsMouseButtonReleased(MouseCode btn) const override { return btn < kNullMaxMouseButtons ? false : false; }
        bool IsMouseButtonUp(MouseCode btn) const override { return btn < kNullMaxMouseButtons ? true : false; }

        std::pair<float, float> GetMousePosition() const override { return {0.0f, 0.0f}; }
        float GetMouseX() const override { return 0.0f; }
        float GetMouseY() const override { return 0.0f; }
        float GetMouseWheelMove() const override { return 0.0f; }

        void AccumulateScroll(float, float) override {}
    };

} // namespace Wayfinder
