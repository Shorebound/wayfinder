#pragma once
#include "../Input.h"

namespace Wayfinder
{
    class RaylibInput : public Input
    {
    public:
        RaylibInput() = default;
        virtual ~RaylibInput() = default;

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
