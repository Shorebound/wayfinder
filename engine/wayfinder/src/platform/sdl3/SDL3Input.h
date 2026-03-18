#pragma once
#include "../Input.h"

#include <array>

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

        void AccumulateScroll(float x, float y) override;

    private:
        static constexpr int kMaxScancodes = 512;
        static constexpr int kMaxMouseButtons = 6; // SDL3: 1-5

        std::array<bool, kMaxScancodes> m_currentKeys{};
        std::array<bool, kMaxScancodes> m_previousKeys{};

        std::array<bool, kMaxMouseButtons> m_currentMouseButtons{};
        std::array<bool, kMaxMouseButtons> m_previousMouseButtons{};

        float m_scrollX = 0.0f;
        float m_scrollY = 0.0f;
    };
}
