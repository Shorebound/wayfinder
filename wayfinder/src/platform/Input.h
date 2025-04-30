#pragma once

#include <utility>
#include <memory>

namespace Wayfinder
{
    // Key and mouse codes will be defined in a separate header
    // to avoid direct dependency on Raylib's key codes

    // Interface for input handling
    class WAYFINDER_API Input
    {
    public:
        virtual ~Input() = default;

        virtual bool IsKeyPressed(int keycode) const = 0;
        virtual bool IsKeyDown(int keycode) const = 0;
        virtual bool IsKeyReleased(int keycode) const = 0;
        virtual bool IsKeyUp(int keycode) const = 0;

        virtual bool IsMouseButtonPressed(int button) const = 0;
        virtual bool IsMouseButtonDown(int button) const = 0;
        virtual bool IsMouseButtonReleased(int button) const = 0;
        virtual bool IsMouseButtonUp(int button) const = 0;

        virtual std::pair<float, float> GetMousePosition() const = 0;
        virtual float GetMouseX() const = 0;
        virtual float GetMouseY() const = 0;
        virtual float GetMouseWheelMove() const = 0;

        static std::unique_ptr<Input> Create();
    };
}
