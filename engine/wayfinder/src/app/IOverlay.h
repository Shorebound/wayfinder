#pragma once

#include "core/Result.h"

#include <string_view>

namespace Wayfinder
{
    class EngineContext;
    class EventQueue;

    /**
     * @brief V2 overlay interface with attach/detach lifecycle.
     *
     * Overlays are persistent UI layers (FPS counter, debug console, etc.)
     * that live across application state transitions. The OverlayStack
     * manages their activation and capability gating.
     */
    class IOverlay
    {
    public:
        virtual ~IOverlay() = default;

        /// Called when the overlay is added to the stack. May fail.
        [[nodiscard]] virtual auto OnAttach(EngineContext& context) -> Result<void> = 0;

        /// Called when the overlay is removed from the stack. May fail.
        [[nodiscard]] virtual auto OnDetach(EngineContext& context) -> Result<void> = 0;

        /// Per-frame update tick.
        virtual void OnUpdate(EngineContext& /*context*/, float /*deltaTime*/) {}

        /// Per-frame render submission.
        virtual void OnRender(EngineContext& /*context*/) {}

        /// Per-frame event processing. Return true if events were consumed (stops propagation).
        virtual auto OnEvent(EngineContext& /*context*/, EventQueue& /*events*/) -> bool
        {
            return false;
        }

        /// Human-readable name for debugging and logging.
        [[nodiscard]] virtual auto GetName() const -> std::string_view = 0;
    };

} // namespace Wayfinder
