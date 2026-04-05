#pragma once

#include "AppDescriptor.h"
#include "ApplicationStateMachine.h"
#include "SubsystemManifest.h"
#include "core/Assert.h"
#include "gameplay/Capability.h"
#include "wayfinder_exports.h"

#include <stop_token>
#include <typeindex>

namespace Wayfinder
{
    class AppSubsystem;
    class EventQueue;
    class IApplicationState;
    class OverlayStack;
    class StateSubsystem;

    /**
     * @brief Non-owning facade providing typed access to engine services.
     *
     * Constructed by Application with pointers to its owned members.
     * Passed by reference to all v2 lifecycle methods (IApplicationState,
     * IOverlay, subsystem Initialise, etc.).
     *
     * Partial construction (nullptr for unused fields) supports headless
     * tests without a full Application bootstrap.
     */
    class WAYFINDER_API EngineContext
    {
    public:
        EngineContext() = default;

        // -- Subsystem access (Get asserts, TryGet returns nullable) --

        /// Access an app-scoped subsystem. Asserts if not found.
        template<typename T>
        auto GetAppSubsystem() -> T&
        {
            WAYFINDER_ASSERT(m_appSubsystems, "App subsystem registry not set");
            return m_appSubsystems->template Get<T>();
        }

        /// Access an app-scoped subsystem, or nullptr if not available.
        template<typename T>
        auto TryGetAppSubsystem() -> T*
        {
            return m_appSubsystems ? m_appSubsystems->template TryGet<T>() : nullptr;
        }

        /// Access a state-scoped subsystem. Asserts if not found.
        template<typename T>
        auto GetStateSubsystem() -> T&
        {
            WAYFINDER_ASSERT(m_stateSubsystems, "State subsystem registry not set");
            return m_stateSubsystems->template Get<T>();
        }

        /// Access a state-scoped subsystem, or nullptr if not available.
        template<typename T>
        auto TryGetStateSubsystem() -> T*
        {
            return m_stateSubsystems ? m_stateSubsystems->template TryGet<T>() : nullptr;
        }

        // -- Const overloads --

        template<typename T>
        auto GetAppSubsystem() const -> const T&
        {
            WAYFINDER_ASSERT(m_appSubsystems, "App subsystem registry not set");
            return m_appSubsystems->template Get<T>();
        }

        template<typename T>
        auto TryGetAppSubsystem() const -> const T*
        {
            return m_appSubsystems ? m_appSubsystems->template TryGet<T>() : nullptr;
        }

        template<typename T>
        auto GetStateSubsystem() const -> const T&
        {
            WAYFINDER_ASSERT(m_stateSubsystems, "State subsystem registry not set");
            return m_stateSubsystems->template Get<T>();
        }

        template<typename T>
        auto TryGetStateSubsystem() const -> const T*
        {
            return m_stateSubsystems ? m_stateSubsystems->template TryGet<T>() : nullptr;
        }

        // -- State transition requests (delegates to ApplicationStateMachine) --

        /// Request a flat state transition to target type.
        template<std::derived_from<IApplicationState> T>
        void RequestTransition()
        {
            WAYFINDER_ASSERT(m_stateMachine, "ApplicationStateMachine not set");
            m_stateMachine->template RequestTransition<T>();
        }

        /// Push a modal state on top of the current state.
        template<std::derived_from<IApplicationState> T>
        void RequestPush()
        {
            WAYFINDER_ASSERT(m_stateMachine, "ApplicationStateMachine not set");
            m_stateMachine->template RequestPush<T>();
        }

        /// Pop the current modal state, resuming the one beneath.
        void RequestPop();

        // -- Overlay control (delegates to OverlayStack) --

        /// Activate an overlay by type.
        void ActivateOverlay(std::type_index overlayType);

        /// Deactivate an overlay by type.
        void DeactivateOverlay(std::type_index overlayType);

        /// Request graceful application shutdown.
        void RequestStop();

        /// Check if stop has been requested.
        [[nodiscard]] auto IsStopRequested() const -> bool;

        /// Obtain a stop_token for cooperative cancellation wiring.
        [[nodiscard]] auto GetStopToken() const -> std::stop_token;

        // -- AppDescriptor access --

        /// Access the immutable application descriptor.
        /// Available after Application::Initialise() completes.
        [[nodiscard]] auto GetAppDescriptor() const -> const AppDescriptor&;

        /// Try to access the application descriptor, or nullptr if not yet set.
        [[nodiscard]] auto TryGetAppDescriptor() const -> const AppDescriptor*;

        // -- Setters (called by Application during construction) --
        /// @todo Phase 6: Revisit access protection when Application class is built (private + friend, or constructor params).

        void SetAppSubsystems(SubsystemManifest<AppSubsystem>* manifest);
        void SetStateSubsystems(SubsystemManifest<StateSubsystem>* manifest);
        void SetAppDescriptor(const AppDescriptor* descriptor);
        void SetStateMachine(ApplicationStateMachine* stateMachine);
        void SetOverlayStack(OverlayStack* overlayStack);

    private:
        SubsystemManifest<AppSubsystem>* m_appSubsystems = nullptr;
        SubsystemManifest<StateSubsystem>* m_stateSubsystems = nullptr;
        const AppDescriptor* m_appDescriptor = nullptr;
        std::stop_source m_stopSource;
        ApplicationStateMachine* m_stateMachine = nullptr;
        OverlayStack* m_overlayStack = nullptr;
    };

    // -- Capability set computation --

    /// Compute the effective capability set from app-level and state-level sources.
    /// The result is the union of both sets. This is a pure function with no side effects.
    /// Used during state transitions to compute the new capability environment.
    [[nodiscard]] inline auto ComputeEffectiveCaps(const CapabilitySet& appCaps, const CapabilitySet& stateCaps) -> CapabilitySet
    {
        CapabilitySet effective = appCaps;
        effective.AddTags(stateCaps);
        return effective;
    }

} // namespace Wayfinder
