#pragma once

#include "OrchestrationTypes.h"
#include "core/Result.h"

#include <string_view>

namespace Wayfinder
{
    class EngineContext;
    class EventQueue;

    /**
     * @brief V2 application state interface with full lifecycle.
     *
     * An application state represents a major mode of the application
     * (e.g. Gameplay, Editor, MainMenu). The ApplicationStateMachine
     * manages transitions between states.
     *
     * OnEnter/OnExit may fail and return an error. Per-frame methods
     * provide empty defaults so states only override what they need.
     */
    class IApplicationState
    {
    public:
        virtual ~IApplicationState() = default;

        /// Called when this state becomes the active state. May fail.
        [[nodiscard]] virtual auto OnEnter(EngineContext& context) -> Result<void> = 0;

        /// Called when this state is being removed. May fail.
        [[nodiscard]] virtual auto OnExit(EngineContext& context) -> Result<void> = 0;

        /// Called when another state is pushed on top of this one.
        virtual void OnSuspend(EngineContext& /*context*/) {}

        /// Called when this state becomes active again after a pop.
        virtual void OnResume(EngineContext& /*context*/) {}

        /// Background preferences when this state is suspended by a push.
        [[nodiscard]] virtual auto GetBackgroundPreferences() const -> BackgroundPreferences
        {
            return {};
        }

        /// Suspension policy this state imposes on background states when pushed.
        [[nodiscard]] virtual auto GetSuspensionPolicy() const -> SuspensionPolicy
        {
            return {};
        }

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
