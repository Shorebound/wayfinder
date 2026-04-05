#include "SDLInputSubsystem.h"

#include "core/Log.h"

#include <SDL3/SDL.h>

#include <algorithm>

namespace
{
    constexpr int SDL_MOUSE_BUTTON_FIRST = 1;
}

namespace Wayfinder
{
    SDLInputSubsystem::SDLInputSubsystem()
    {
        m_currentKeys.fill(false);
        m_previousKeys.fill(false);
        m_currentMouseButtons.fill(false);
        m_previousMouseButtons.fill(false);
    }

    auto SDLInputSubsystem::Initialise([[maybe_unused]] EngineContext& context) -> Result<void>
    {
        Log::Info(LogEngine, "SDLInputSubsystem: Initialised");
        return {};
    }

    void SDLInputSubsystem::Shutdown()
    {
        Log::Info(LogEngine, "SDLInputSubsystem: Shutting down");
    }

    void SDLInputSubsystem::BeginFrame()
    {
        // Snapshot previous state
        m_previousKeys = m_currentKeys;
        m_previousMouseButtons = m_currentMouseButtons;

        // Sample current keyboard state (scancodes index directly)
        m_currentKeys.fill(false);
        int numKeys = 0;
        const bool* sdlKeys = SDL_GetKeyboardState(&numKeys);
        if (sdlKeys and numKeys > 0)
        {
            const auto keyCount = std::min(static_cast<std::size_t>(numKeys), m_currentKeys.size());
            std::copy_n(sdlKeys, keyCount, m_currentKeys.begin());
        }

        // Sample current mouse button state
        m_currentMouseButtons.fill(false);
        const SDL_MouseButtonFlags mouseState = SDL_GetMouseState(nullptr, nullptr);
        for (auto button = static_cast<std::size_t>(SDL_MOUSE_BUTTON_FIRST); button < m_currentMouseButtons.size(); ++button)
        {
            m_currentMouseButtons[button] = (mouseState & SDL_BUTTON_MASK(static_cast<int>(button))) != 0;
        }

        // Reset scroll accumulator
        m_scrollX = 0.0f;
        m_scrollY = 0.0f;
    }

    auto SDLInputSubsystem::IsKeyPressed(KeyCode key) const -> bool
    {
        return GetKeyState(m_currentKeys, key) and not GetKeyState(m_previousKeys, key);
    }

    auto SDLInputSubsystem::IsKeyDown(KeyCode key) const -> bool
    {
        return GetKeyState(m_currentKeys, key);
    }

    auto SDLInputSubsystem::IsKeyReleased(KeyCode key) const -> bool
    {
        return not GetKeyState(m_currentKeys, key) and GetKeyState(m_previousKeys, key);
    }

    auto SDLInputSubsystem::IsKeyUp(KeyCode key) const -> bool
    {
        return not GetKeyState(m_currentKeys, key);
    }

    auto SDLInputSubsystem::IsMouseButtonPressed(MouseCode button) const -> bool
    {
        return GetMouseButtonState(m_currentMouseButtons, button) and not GetMouseButtonState(m_previousMouseButtons, button);
    }

    auto SDLInputSubsystem::IsMouseButtonDown(MouseCode button) const -> bool
    {
        return GetMouseButtonState(m_currentMouseButtons, button);
    }

    auto SDLInputSubsystem::IsMouseButtonReleased(MouseCode button) const -> bool
    {
        return not GetMouseButtonState(m_currentMouseButtons, button) and GetMouseButtonState(m_previousMouseButtons, button);
    }

    auto SDLInputSubsystem::IsMouseButtonUp(MouseCode button) const -> bool
    {
        return not GetMouseButtonState(m_currentMouseButtons, button);
    }

    auto SDLInputSubsystem::GetMousePosition() const -> std::pair<float, float>
    {
        float x = 0.0f;
        float y = 0.0f;
        SDL_GetMouseState(&x, &y);
        return {x, y};
    }

    auto SDLInputSubsystem::GetMouseX() const -> float
    {
        float x = 0.0f;
        SDL_GetMouseState(&x, nullptr);
        return x;
    }

    auto SDLInputSubsystem::GetMouseY() const -> float
    {
        float y = 0.0f;
        SDL_GetMouseState(nullptr, &y);
        return y;
    }

    auto SDLInputSubsystem::GetMouseWheelMove() const -> float
    {
        return m_scrollY;
    }

    void SDLInputSubsystem::AccumulateScroll(ScrollDelta delta)
    {
        m_scrollX += delta.X;
        m_scrollY += delta.Y;
    }

    auto SDLInputSubsystem::TryGetKeyIndex(KeyCode key) -> std::optional<std::size_t>
    {
        if (not Key::IsValid(key))
        {
            return std::nullopt;
        }

        return static_cast<std::size_t>(key);
    }

    auto SDLInputSubsystem::TryGetMouseButtonIndex(MouseCode button) -> std::optional<std::size_t>
    {
        if (not Mouse::IsValid(button))
        {
            return std::nullopt;
        }

        return static_cast<std::size_t>(button);
    }

    auto SDLInputSubsystem::GetKeyState(const std::array<bool, Key::STATE_COUNT>& states, KeyCode key) const -> bool
    {
        const auto index = TryGetKeyIndex(key);
        if (not index.has_value())
        {
            return false;
        }

        return states.at(*index);
    }

    auto SDLInputSubsystem::GetMouseButtonState(const std::array<bool, Mouse::BUTTON_STATE_COUNT>& states, MouseCode button) const -> bool
    {
        const auto index = TryGetMouseButtonIndex(button);
        if (not index.has_value())
        {
            return false;
        }

        return states.at(*index);
    }

} // namespace Wayfinder
