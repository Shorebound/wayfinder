#include "RaylibInput.h"
#include "raylib.h"

namespace Wayfinder
{
    std::unique_ptr<Input> Input::Create()
    {
        return std::make_unique<RaylibInput>();
    }

    bool RaylibInput::IsKeyPressed(int keycode) const
    {
        return ::IsKeyPressed(keycode);
    }

    bool RaylibInput::IsKeyDown(int keycode) const
    {
        return ::IsKeyDown(keycode);
    }

    bool RaylibInput::IsKeyReleased(int keycode) const
    {
        return ::IsKeyReleased(keycode);
    }

    bool RaylibInput::IsKeyUp(int keycode) const
    {
        return ::IsKeyUp(keycode);
    }

    bool RaylibInput::IsMouseButtonPressed(int button) const
    {
        return ::IsMouseButtonPressed(button);
    }

    bool RaylibInput::IsMouseButtonDown(int button) const
    {
        return ::IsMouseButtonDown(button);
    }

    bool RaylibInput::IsMouseButtonReleased(int button) const
    {
        return ::IsMouseButtonReleased(button);
    }

    bool RaylibInput::IsMouseButtonUp(int button) const
    {
        return ::IsMouseButtonUp(button);
    }

    std::pair<float, float> RaylibInput::GetMousePosition() const
    {
        Vector2 pos = ::GetMousePosition();
        return {pos.x, pos.y};
    }

    float RaylibInput::GetMouseX() const
    {
        return ::GetMouseX();
    }

    float RaylibInput::GetMouseY() const
    {
        return ::GetMouseY();
    }

    float RaylibInput::GetMouseWheelMove() const
    {
        return ::GetMouseWheelMove();
    }
}
