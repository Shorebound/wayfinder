#pragma once
#include "platform/Input.h"

#include <array>
#include <optional>

namespace Wayfinder
{
    class SDL3Input : public Input
    {
    public:
        SDL3Input();
        ~SDL3Input() override = default;

        void BeginFrame() override;

        bool IsKeyPressed(KeyCode key) const override;
        bool IsKeyDown(KeyCode key) const override;
        bool IsKeyReleased(KeyCode key) const override;
        bool IsKeyUp(KeyCode key) const override;

        bool IsMouseButtonPressed(MouseCode button) const override;
        bool IsMouseButtonDown(MouseCode button) const override;
        bool IsMouseButtonReleased(MouseCode button) const override;
        bool IsMouseButtonUp(MouseCode button) const override;

        std::pair<float, float> GetMousePosition() const override;
        float GetMouseX() const override;
        float GetMouseY() const override;
        float GetMouseWheelMove() const override;

        void AccumulateScroll(ScrollDelta delta) override;

    private:
        static std::optional<std::size_t> TryGetKeyIndex(KeyCode key);
        static std::optional<std::size_t> TryGetMouseButtonIndex(MouseCode button);

        bool GetKeyState(const std::array<bool, Key::STATE_COUNT>& states, KeyCode key) const;
        bool GetMouseButtonState(const std::array<bool, Mouse::BUTTON_STATE_COUNT>& states, MouseCode button) const;

        std::array<bool, Key::STATE_COUNT> m_currentKeys{};
        std::array<bool, Key::STATE_COUNT> m_previousKeys{};

        std::array<bool, Mouse::BUTTON_STATE_COUNT> m_currentMouseButtons{};
        std::array<bool, Mouse::BUTTON_STATE_COUNT> m_previousMouseButtons{};

        float m_scrollX = 0.0f;
        float m_scrollY = 0.0f;
    };
}
