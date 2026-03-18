#include "SDL3Input.h"

#include <SDL3/SDL.h>

namespace Wayfinder
{
    std::unique_ptr<Input> Input::Create(PlatformBackend backend)
    {
        switch (backend)
        {
        case PlatformBackend::SDL3:
            return std::make_unique<SDL3Input>();
        }

        return nullptr;
    }

    bool SDL3Input::IsKeyPressed(int keycode) const
    {
        // SDL3 doesn't have a built-in "pressed this frame" concept.
        // This will be implemented with per-frame state tracking later.
        return false;
    }

    bool SDL3Input::IsKeyDown(int keycode) const
    {
        const bool* state = SDL_GetKeyboardState(nullptr);
        if (!state)
        {
            return false;
        }

        SDL_Scancode scancode = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(keycode), nullptr);
        return state[scancode];
    }

    bool SDL3Input::IsKeyReleased(int keycode) const
    {
        return false;
    }

    bool SDL3Input::IsKeyUp(int keycode) const
    {
        return !IsKeyDown(keycode);
    }

    bool SDL3Input::IsMouseButtonPressed(int button) const
    {
        return false;
    }

    bool SDL3Input::IsMouseButtonDown(int button) const
    {
        SDL_MouseButtonFlags state = SDL_GetMouseState(nullptr, nullptr);
        return (state & SDL_BUTTON_MASK(button)) != 0;
    }

    bool SDL3Input::IsMouseButtonReleased(int button) const
    {
        return false;
    }

    bool SDL3Input::IsMouseButtonUp(int button) const
    {
        return !IsMouseButtonDown(button);
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
        // Wheel events are handled per-frame via SDL_PollEvent.
        // This will be integrated with the event system later.
        return 0.0f;
    }
}
