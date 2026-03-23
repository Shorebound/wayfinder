#include "SDL3Input.h"
#include "platform/null/NullInput.h"

#include <SDL3/SDL.h>
#include <cstring>

namespace Wayfinder
{
    std::unique_ptr<Input> Input::Create(PlatformBackend backend)
    {
        switch (backend)
        {
        case PlatformBackend::SDL3:
            return std::make_unique<SDL3Input>();
        case PlatformBackend::Null:
            return std::make_unique<NullInput>();
        }

        return nullptr;
    }

    SDL3Input::SDL3Input()
    {
        m_currentKeys.fill(false);
        m_previousKeys.fill(false);
        m_currentMouseButtons.fill(false);
        m_previousMouseButtons.fill(false);
    }

    void SDL3Input::BeginFrame()
    {
        // Snapshot previous state
        m_previousKeys = m_currentKeys;
        m_previousMouseButtons = m_currentMouseButtons;

        // Sample current keyboard state (scancodes index directly)
        int numKeys = 0;
        const bool* sdlKeys = SDL_GetKeyboardState(&numKeys);
        if (sdlKeys)
        {
            const int count = (numKeys < kMaxScancodes) ? numKeys : kMaxScancodes;
            for (int i = 0; i < count; ++i)
                m_currentKeys[i] = sdlKeys[i];
        }

        // Sample current mouse button state
        SDL_MouseButtonFlags mouseState = SDL_GetMouseState(nullptr, nullptr);
        for (int i = 1; i < kMaxMouseButtons; ++i)
            m_currentMouseButtons[i] = (mouseState & SDL_BUTTON_MASK(i)) != 0;

        // Reset scroll accumulator
        m_scrollX = 0.0f;
        m_scrollY = 0.0f;
    }

    bool SDL3Input::IsKeyPressed(KeyCode key) const
    {
        if (key >= kMaxScancodes) return false;
        return m_currentKeys[key] && !m_previousKeys[key];
    }

    bool SDL3Input::IsKeyDown(KeyCode key) const
    {
        if (key >= kMaxScancodes) return false;
        return m_currentKeys[key];
    }

    bool SDL3Input::IsKeyReleased(KeyCode key) const
    {
        if (key >= kMaxScancodes) return false;
        return !m_currentKeys[key] && m_previousKeys[key];
    }

    bool SDL3Input::IsKeyUp(KeyCode key) const
    {
        if (key >= kMaxScancodes) return false;
        return !m_currentKeys[key];
    }

    bool SDL3Input::IsMouseButtonPressed(MouseCode button) const
    {
        if (button >= kMaxMouseButtons) return false;
        return m_currentMouseButtons[button] && !m_previousMouseButtons[button];
    }

    bool SDL3Input::IsMouseButtonDown(MouseCode button) const
    {
        if (button >= kMaxMouseButtons) return false;
        return m_currentMouseButtons[button];
    }

    bool SDL3Input::IsMouseButtonReleased(MouseCode button) const
    {
        if (button >= kMaxMouseButtons) return false;
        return !m_currentMouseButtons[button] && m_previousMouseButtons[button];
    }

    bool SDL3Input::IsMouseButtonUp(MouseCode button) const
    {
        if (button >= kMaxMouseButtons) return false;
        return !m_currentMouseButtons[button];
    }

    std::pair<float, float> SDL3Input::GetMousePosition() const
    {
        float x = 0.0f, y = 0.0f;
        SDL_GetMouseState(&x, &y);
        return {x, y};
    }

    float SDL3Input::GetMouseX() const
    {
        float x = 0.0f;
        SDL_GetMouseState(&x, nullptr);
        return x;
    }

    float SDL3Input::GetMouseY() const
    {
        float y = 0.0f;
        SDL_GetMouseState(nullptr, &y);
        return y;
    }

    float SDL3Input::GetMouseWheelMove() const
    {
        return m_scrollY;
    }

    void SDL3Input::AccumulateScroll(float x, float y)
    {
        m_scrollX += x;
        m_scrollY += y;
    }
}
