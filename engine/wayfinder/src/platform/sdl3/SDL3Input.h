#pragma once
#include "../Input.h"

namespace Wayfinder
{
    class SDL3Input : public Input
    {
    public:
        SDL3Input() = default;
        ~SDL3Input() override = default;

        bool IsKeyPressed(int keycode) const override;
        bool IsKeyDown(int keycode) const override;
        bool IsKeyReleased(int keycode) const override;
        bool IsKeyUp(int keycode) const override;

        bool IsMouseButtonPressed(int button) const override;
        bool IsMouseButtonDown(int button) const override;
        bool IsMouseButtonReleased(int button) const override;
        bool IsMouseButtonUp(int button) const override;

        std::pair<float, float> GetMousePosition() const override;
        float GetMouseX() const override;
        float GetMouseY() const override;
        float GetMouseWheelMove() const override;
    };
}
