#pragma once

#include "app/Layer.h"

namespace Wayfinder
{
    class Window;

    /**
     * @brief Updates the window title with averaged FPS and frame time (ms).
     *
     * Uses the title bar instead of drawing text — the renderer has no font/UI path yet.
     * Enable from `Application` in non-Shipping debug builds.
     */
    class FpsOverlayLayer final : public Layer
    {
    public:
        explicit FpsOverlayLayer(Window& window);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate(float deltaTime) override;

        std::string_view GetName() const override
        {
            return "FpsOverlay";
        }

    private:
        Window* m_window = nullptr;
        std::string m_baseTitle;
        float m_accumSeconds = 0.0f;
        uint32_t m_framesInWindow = 0;
    };

} // namespace Wayfinder
