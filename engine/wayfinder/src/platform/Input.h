#pragma once

#include "../core/BackendConfig.h"
#include "../core/KeyCodes.h"
#include "../core/MouseCodes.h"

#include <utility>
#include <memory>

namespace Wayfinder
{
    class WAYFINDER_API Input
    {
    public:
        virtual ~Input() = default;

        // Called once per frame before event polling to snapshot previous state
        virtual void BeginFrame() = 0;

        virtual bool IsKeyPressed(KeyCode key) const = 0;
        virtual bool IsKeyDown(KeyCode key) const = 0;
        virtual bool IsKeyReleased(KeyCode key) const = 0;
        virtual bool IsKeyUp(KeyCode key) const = 0;

        virtual bool IsMouseButtonPressed(MouseCode button) const = 0;
        virtual bool IsMouseButtonDown(MouseCode button) const = 0;
        virtual bool IsMouseButtonReleased(MouseCode button) const = 0;
        virtual bool IsMouseButtonUp(MouseCode button) const = 0;

        virtual std::pair<float, float> GetMousePosition() const = 0;
        virtual float GetMouseX() const = 0;
        virtual float GetMouseY() const = 0;
        virtual float GetMouseWheelMove() const = 0;

        // Called by the window layer to accumulate scroll events during polling
        virtual void AccumulateScroll(float x, float y) = 0;

        static std::unique_ptr<Input> Create(PlatformBackend backend = PlatformBackend::SDL3);
    };
}
