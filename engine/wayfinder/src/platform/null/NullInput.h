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
            return GetDefaultKeyState(key, false);
        }
        bool IsKeyDown(KeyCode key) const override
        {
            return GetDefaultKeyState(key, false);
        }
        bool IsKeyReleased(KeyCode key) const override
        {
            return GetDefaultKeyState(key, false);
        }
        bool IsKeyUp(KeyCode key) const override
        {
            return GetDefaultKeyState(key, true);
        }

        bool IsMouseButtonPressed(MouseCode btn) const override
        {
            return GetDefaultMouseButtonState(btn, false);
        }
        bool IsMouseButtonDown(MouseCode btn) const override
        {
            return GetDefaultMouseButtonState(btn, false);
        }
        bool IsMouseButtonReleased(MouseCode btn) const override
        {
            return GetDefaultMouseButtonState(btn, false);
        }
        bool IsMouseButtonUp(MouseCode btn) const override
        {
            return GetDefaultMouseButtonState(btn, true);
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

    private:
        static bool GetDefaultKeyState(KeyCode key, bool isUpState)
        {
            if (!Key::IsValid(key))
            {
                return false;
            }

            return isUpState;
        }

        static bool GetDefaultMouseButtonState(MouseCode button, bool isUpState)
        {
            if (!Mouse::IsValid(button))
            {
                return false;
            }

            return isUpState;
        }
    };

} // namespace Wayfinder
