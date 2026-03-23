#pragma once

#include "platform/Input.h"

namespace Wayfinder
{
    class NullInput final : public Input
    {
    public:
        void BeginFrame() override {}

        bool IsKeyPressed(KeyCode key) const override
        {
            if (!Key::IsValid(key))
            {
                return false;
            }

            return false;
        }
        bool IsKeyDown(KeyCode key) const override
        {
            if (!Key::IsValid(key))
            {
                return false;
            }

            return false;
        }
        bool IsKeyReleased(KeyCode key) const override
        {
            if (!Key::IsValid(key))
            {
                return false;
            }

            return false;
        }
        bool IsKeyUp(KeyCode key) const override
        {
            return Key::IsValid(key);
        }

        bool IsMouseButtonPressed(MouseCode btn) const override
        {
            if (!Mouse::IsValid(btn))
            {
                return false;
            }

            return false;
        }
        bool IsMouseButtonDown(MouseCode btn) const override
        {
            if (!Mouse::IsValid(btn))
            {
                return false;
            }

            return false;
        }
        bool IsMouseButtonReleased(MouseCode btn) const override
        {
            if (!Mouse::IsValid(btn))
            {
                return false;
            }

            return false;
        }
        bool IsMouseButtonUp(MouseCode btn) const override
        {
            return Mouse::IsValid(btn);
        }

        std::pair<float, float> GetMousePosition() const override
        {
            return {0.0f, 0.0f};
        }
        float GetMouseX() const override
        {
            return 0.0f;
        }
        float GetMouseY() const override
        {
            return 0.0f;
        }
        float GetMouseWheelMove() const override
        {
            return 0.0f;
        }

        void AccumulateScroll(ScrollDelta) override {}
    };

} // namespace Wayfinder
