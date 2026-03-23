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
        static constexpr std::size_t kMaxScancodes = 512;
        static constexpr std::size_t kMaxMouseButtons = 6; // SDL3: 1-5

        static std::optional<std::size_t> TryGetKeyIndex(KeyCode key);
        static std::optional<std::size_t> TryGetMouseButtonIndex(MouseCode button);

        bool GetKeyState(const std::array<bool, kMaxScancodes>& states, KeyCode key) const;
        bool GetMouseButtonState(const std::array<bool, kMaxMouseButtons>& states, MouseCode button) const;

        std::array<bool, kMaxScancodes> m_currentKeys{};
        std::array<bool, kMaxScancodes> m_previousKeys{};

        std::array<bool, kMaxMouseButtons> m_currentMouseButtons{};
        std::array<bool, kMaxMouseButtons> m_previousMouseButtons{};

        float m_scrollX = 0.0f;
        float m_scrollY = 0.0f;
    };
}
