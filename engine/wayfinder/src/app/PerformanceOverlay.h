#pragma once

#include "IOverlay.h"

#include <cstdint>

namespace Wayfinder
{
    /**
     * @brief Performance metrics overlay rendering FPS and frame time via ImGui.
     *
     * Replaces FpsOverlayLayer (v1) as the first ImGui consumer through the
     * v2 overlay system. Accumulates frame timing data and refreshes display
     * values at ~4Hz for human-readable, stable output.
     *
     * Requires Presentation capability. Disabled in Shipping builds.
     */
    class PerformanceOverlay final : public IOverlay
    {
    public:
        [[nodiscard]] auto OnAttach(EngineContext& context) -> Result<void> override;
        [[nodiscard]] auto OnDetach(EngineContext& context) -> Result<void> override;

        void OnUpdate(EngineContext& context, float deltaTime) override;
        void OnRender(EngineContext& context) override;

        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "PerformanceOverlay";
        }

        // -- Test accessors ---------------------------------------------------

        [[nodiscard]] auto GetDisplayFps() const -> float { return m_displayFps; }
        [[nodiscard]] auto GetDisplayMs() const -> float { return m_displayMs; }

    private:
        /// Refresh interval in seconds (~4 updates/second for stable readability).
        static constexpr float REFRESH_INTERVAL = 0.25f;

        float m_accumSeconds = 0.0f;
        uint32_t m_frameCount = 0;
        float m_displayFps = 0.0f;
        float m_displayMs = 0.0f;
    };

} // namespace Wayfinder
