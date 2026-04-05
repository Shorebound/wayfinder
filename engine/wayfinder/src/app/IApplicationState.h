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

        /**
         * @brief Called when this state becomes the active state.
         * @param context Engine services and subsystem access.
         * @return Success, or an error if entering the state failed.
         */
        [[nodiscard]] virtual auto OnEnter(EngineContext& context) -> Result<void> = 0;

        /**
         * @brief Called when this state is being removed.
         * @param context Engine services and subsystem access.
         * @return Success, or an error if exiting the state failed.
         */
        [[nodiscard]] virtual auto OnExit(EngineContext& context) -> Result<void> = 0;

        /** @brief Called when another state is pushed on top of this one. */
        virtual void OnSuspend(EngineContext& /*context*/) {}

        /** @brief Called when this state becomes active again after a pop. */
        virtual void OnResume(EngineContext& /*context*/) {}

        /**
         * @brief Background preferences when this state is suspended by a push.
         * @return The background mode this state requests while suspended.
         */
        [[nodiscard]] virtual auto GetBackgroundPreferences() const -> BackgroundMode
        {
            return BackgroundMode::None;
        }

        /**
         * @brief Suspension policy this state imposes on background states when pushed.
         * @return The background mode this state allows for the state beneath it.
         */
        [[nodiscard]] virtual auto GetSuspensionPolicy() const -> BackgroundMode
        {
            return BackgroundMode::Render;
        }

        /** @brief Per-frame update tick. */
        virtual void OnUpdate(EngineContext& /*context*/, float /*deltaTime*/) {}

        /** @brief Per-frame render submission. */
        virtual void OnRender(EngineContext& /*context*/) {}

        /** @brief Per-frame event processing. */
        virtual void OnEvent(EngineContext& /*context*/, EventQueue& /*events*/) {}

        /**
         * @brief Human-readable name for debugging and logging.
         * @return A string view identifying this state.
         */
        [[nodiscard]] virtual auto GetName() const -> std::string_view = 0;
    };

} // namespace Wayfinder
