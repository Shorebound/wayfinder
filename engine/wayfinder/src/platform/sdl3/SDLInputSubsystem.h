#pragma once

#include "app/AppSubsystem.h"
#include "core/Result.h"
#include "platform/KeyCodes.h"
#include "platform/MouseCodes.h"
#include "wayfinder_exports.h"

#include <array>
#include <optional>
#include <utility>

namespace Wayfinder
{
    class EngineContext;

    /**
     * @brief Direct SDL3 input management as an AppSubsystem.
     *
     * Queries SDL keyboard and mouse state each frame. Replaces the
     * three-tier Input (abstract) + SDL3Input (impl) + InputSubsystem (wrapper)
     * with a single concrete type. Always active (no required capabilities).
     */
    class WAYFINDER_API SDLInputSubsystem final : public AppSubsystem
    {
    public:
        struct ScrollDelta
        {
            float X = 0.0f;
            float Y = 0.0f;
        };

        SDLInputSubsystem();
        ~SDLInputSubsystem() override = default;

        [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
        void Shutdown() override;

        /// Called once per frame before event polling to snapshot previous state.
        void BeginFrame();

        [[nodiscard]] auto IsKeyPressed(KeyCode key) const -> bool;
        [[nodiscard]] auto IsKeyDown(KeyCode key) const -> bool;
        [[nodiscard]] auto IsKeyReleased(KeyCode key) const -> bool;
        [[nodiscard]] auto IsKeyUp(KeyCode key) const -> bool;

        [[nodiscard]] auto IsMouseButtonPressed(MouseCode button) const -> bool;
        [[nodiscard]] auto IsMouseButtonDown(MouseCode button) const -> bool;
        [[nodiscard]] auto IsMouseButtonReleased(MouseCode button) const -> bool;
        [[nodiscard]] auto IsMouseButtonUp(MouseCode button) const -> bool;

        [[nodiscard]] auto GetMousePosition() const -> std::pair<float, float>;
        [[nodiscard]] auto GetMouseX() const -> float;
        [[nodiscard]] auto GetMouseY() const -> float;
        [[nodiscard]] auto GetMouseWheelMove() const -> float;

        void AccumulateScroll(ScrollDelta delta);

    private:
        static auto TryGetKeyIndex(KeyCode key) -> std::optional<std::size_t>;
        static auto TryGetMouseButtonIndex(MouseCode button) -> std::optional<std::size_t>;

        [[nodiscard]] auto GetKeyState(const std::array<bool, Key::STATE_COUNT>& states, KeyCode key) const -> bool;
        [[nodiscard]] auto GetMouseButtonState(const std::array<bool, Mouse::BUTTON_STATE_COUNT>& states, MouseCode button) const -> bool;

        std::array<bool, Key::STATE_COUNT> m_currentKeys{};
        std::array<bool, Key::STATE_COUNT> m_previousKeys{};

        std::array<bool, Mouse::BUTTON_STATE_COUNT> m_currentMouseButtons{};
        std::array<bool, Mouse::BUTTON_STATE_COUNT> m_previousMouseButtons{};

        float m_scrollX = 0.0f;
        float m_scrollY = 0.0f;
    };

} // namespace Wayfinder
