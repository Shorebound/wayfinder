#pragma once

#include "SubsystemRegistry.h"
#include "core/Assert.h"
#include "gameplay/Capability.h"
#include "wayfinder_exports.h"

#include <typeindex>

namespace Wayfinder
{
    class AppSubsystem;
    class EventQueue;
    class IApplicationState;
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

        // -- Phase 4 stubs (assert if called before implementation) --

        /// @prototype Phase 4 - request a flat state transition.
        template<std::derived_from<IApplicationState> T>
        void RequestTransition()
        {
            WAYFINDER_ASSERT(false, "RequestTransition not yet implemented (Phase 4)");
        }

        /// @prototype Phase 4 - push a modal state.
        template<std::derived_from<IApplicationState> T>
        void RequestPush()
        {
            WAYFINDER_ASSERT(false, "RequestPush not yet implemented (Phase 4)");
        }

        /// @prototype Phase 4 - pop the current modal state.
        void RequestPop();

        /// @prototype Phase 4 - activate an overlay by type.
        void ActivateOverlay(std::type_index overlayType);

        /// @prototype Phase 4 - deactivate an overlay by type.
        void DeactivateOverlay(std::type_index overlayType);

        /// Request graceful application shutdown.
        void RequestStop();

        /// Check if stop has been requested.
        [[nodiscard]] auto IsStopRequested() const -> bool;

        // -- Setters (called by Application during construction) --

        void SetAppSubsystems(SubsystemRegistry<AppSubsystem>* registry);
        void SetStateSubsystems(SubsystemRegistry<StateSubsystem>* registry);

    private:
        SubsystemRegistry<AppSubsystem>* m_appSubsystems = nullptr;
        SubsystemRegistry<StateSubsystem>* m_stateSubsystems = nullptr;
        bool m_stopRequested = false;
        // Phase 4 additions (nullptr initially):
        // ApplicationStateMachine* m_stateMachine = nullptr;
        // OverlayStack* m_overlayStack = nullptr;
        // AppDescriptor* m_appDescriptor = nullptr;
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
