#pragma once

#include "../Input.h"

namespace Wayfinder
{
    class NullInput final : public Input
    {
    public:
        void BeginFrame() override {}

        bool IsKeyPressed(KeyCode) const override { return false; }
        bool IsKeyDown(KeyCode) const override { return false; }
        bool IsKeyReleased(KeyCode) const override { return false; }
        bool IsKeyUp(KeyCode) const override { return true; }

        bool IsMouseButtonPressed(MouseCode) const override { return false; }
        bool IsMouseButtonDown(MouseCode) const override { return false; }
        bool IsMouseButtonReleased(MouseCode) const override { return false; }
        bool IsMouseButtonUp(MouseCode) const override { return true; }

        std::pair<float, float> GetMousePosition() const override { return {0.0f, 0.0f}; }
        float GetMouseX() const override { return 0.0f; }
        float GetMouseY() const override { return 0.0f; }
        float GetMouseWheelMove() const override { return 0.0f; }

        void AccumulateScroll(float, float) override {}
    };

} // namespace Wayfinder
