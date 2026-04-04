#pragma once

#include "core/Result.h"

#include <string_view>

namespace Wayfinder
{
    class EngineContext;
    class EventQueue;

    /**
     * @brief V2 per-state UI interface.
     *
     * IStateUI instances are injected by plugins to provide UI specific
     * to an ApplicationState. Their lifecycle mirrors the state they are
     * attached to.
     */
    class IStateUI
    {
    public:
        virtual ~IStateUI() = default;

        /// Called when the owning state enters. May fail.
        [[nodiscard]] virtual auto OnAttach(EngineContext& context) -> Result<void> = 0;

        /// Called when the owning state exits. May fail.
        [[nodiscard]] virtual auto OnDetach(EngineContext& context) -> Result<void> = 0;

        /// Called when the owning state is suspended.
        virtual void OnSuspend(EngineContext& /*context*/) {}

        /// Called when the owning state resumes.
        virtual void OnResume(EngineContext& /*context*/) {}

        /// Per-frame update tick.
        virtual void OnUpdate(EngineContext& /*context*/, float /*deltaTime*/) {}

        /// Per-frame render submission.
        virtual void OnRender(EngineContext& /*context*/) {}

        /// Per-frame event processing.
        virtual void OnEvent(EngineContext& /*context*/, EventQueue& /*events*/) {}

        /// Human-readable name for debugging and logging.
        [[nodiscard]] virtual auto GetName() const -> std::string_view = 0;
    };

} // namespace Wayfinder
