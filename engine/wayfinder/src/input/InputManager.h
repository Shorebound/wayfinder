#pragma once

#include "raylib.h"

namespace Wayfinder
{

    class WAYFINDER_API InputManager
    {
    public:
        InputManager();
        ~InputManager();

        bool Initialize();
        void Update();
        void Shutdown();

        bool IsKeyPressed(int key) const;
        bool IsKeyDown(int key) const;
        bool IsKeyReleased(int key) const;
        bool IsKeyUp(int key) const;

        bool IsMouseButtonPressed(int button) const;
        bool IsMouseButtonDown(int button) const;
        bool IsMouseButtonReleased(int button) const;
        bool IsMouseButtonUp(int button) const;
        Vector2 GetMousePosition() const;
        Vector2 GetMouseDelta() const;
        float GetMouseWheelMove() const;

    private:
        bool m_isInitialized;
    };

} // namespace Wayfinder
