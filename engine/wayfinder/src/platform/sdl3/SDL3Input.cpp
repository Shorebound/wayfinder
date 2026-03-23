#include "SDL3Input.h"
#include "platform/null/NullInput.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <iterator>

namespace
{
    constexpr int SDL_MOUSE_BUTTON_FIRST = 1;
}

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
        m_currentKeys.fill(false);
        int numKeys = 0;
        const bool* sdlKeys = SDL_GetKeyboardState(&numKeys);
        if (sdlKeys && numKeys > 0)
        {
            const auto keyCount = std::min(static_cast<std::size_t>(numKeys), m_currentKeys.size());
            std::copy_n(sdlKeys, keyCount, m_currentKeys.begin());
        }

        // Sample current mouse button state
        m_currentMouseButtons.fill(false);
        const SDL_MouseButtonFlags mouseState = SDL_GetMouseState(nullptr, nullptr);
        int button = SDL_MOUSE_BUTTON_FIRST;
        for (auto buttonState = std::next(m_currentMouseButtons.begin()); buttonState != m_currentMouseButtons.end(); ++buttonState, ++button)
        {
            *buttonState = (mouseState & SDL_BUTTON_MASK(button)) != 0;
        }

        // Reset scroll accumulator
        m_scrollX = 0.0f;
        m_scrollY = 0.0f;
    }

    bool SDL3Input::IsKeyPressed(KeyCode key) const
    {
        return GetKeyState(m_currentKeys, key) && !GetKeyState(m_previousKeys, key);
    }

    bool SDL3Input::IsKeyDown(KeyCode key) const
    {
        return GetKeyState(m_currentKeys, key);
    }

    bool SDL3Input::IsKeyReleased(KeyCode key) const
    {
        return !GetKeyState(m_currentKeys, key) && GetKeyState(m_previousKeys, key);
    }

    bool SDL3Input::IsKeyUp(KeyCode key) const
    {
        return !GetKeyState(m_currentKeys, key);
    }

    bool SDL3Input::IsMouseButtonPressed(MouseCode button) const
    {
        return GetMouseButtonState(m_currentMouseButtons, button) && !GetMouseButtonState(m_previousMouseButtons, button);
    }

    bool SDL3Input::IsMouseButtonDown(MouseCode button) const
    {
        return GetMouseButtonState(m_currentMouseButtons, button);
    }

    bool SDL3Input::IsMouseButtonReleased(MouseCode button) const
    {
        return !GetMouseButtonState(m_currentMouseButtons, button) && GetMouseButtonState(m_previousMouseButtons, button);
    }

    bool SDL3Input::IsMouseButtonUp(MouseCode button) const
    {
        return !GetMouseButtonState(m_currentMouseButtons, button);
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

    void SDL3Input::AccumulateScroll(ScrollDelta delta)
    {
        m_scrollX += delta.X;
        m_scrollY += delta.Y;
    }

    std::optional<std::size_t> SDL3Input::TryGetKeyIndex(KeyCode key)
    {
        const auto index = static_cast<std::size_t>(key);
        if (index >= kMaxScancodes)
        {
            return std::nullopt;
        }

        return index;
    }

    std::optional<std::size_t> SDL3Input::TryGetMouseButtonIndex(MouseCode button)
    {
        const auto index = static_cast<std::size_t>(button);
        if (index >= kMaxMouseButtons)
        {
            return std::nullopt;
        }

        return index;
    }

    bool SDL3Input::GetKeyState(const std::array<bool, kMaxScancodes>& states, KeyCode key) const
    {
        const auto index = TryGetKeyIndex(key);
        if (!index.has_value())
        {
            return false;
        }

        return states.at(*index);
    }

    bool SDL3Input::GetMouseButtonState(const std::array<bool, kMaxMouseButtons>& states, MouseCode button) const
    {
        const auto index = TryGetMouseButtonIndex(button);
        if (!index.has_value())
        {
            return false;
        }

        return states.at(*index);
    }
}
