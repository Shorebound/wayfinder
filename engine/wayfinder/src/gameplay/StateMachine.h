#pragma once

#include "core/Result.h"

#include <cassert>
#include <format>
#include <functional>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wayfinder
{

    /**
     * @brief Descriptor for a single state in a StateMachine.
     *
     * Each descriptor declares the state's identity, optional lifecycle
     * callbacks, and the set of states it may transition to.
     */
    template<typename TStateId>
    struct StateDescriptor
    {
        TStateId Id;
        std::function<void()> OnEnter;
        std::function<void()> OnExit;
        std::vector<TStateId> AllowedTransitions;
    };

    /**
     * @brief Generic, framework-agnostic flat state machine.
     *
     * Descriptor-based registration with graph validation at Finalise time,
     * deferred transitions (TransitionTo + ProcessPending), and transition
     * observers.
     *
     * @tparam TStateId  The state identifier type (e.g. InternedString, enum class).
     * @tparam THash     Hash functor for TStateId. Defaults to std::hash<TStateId>.
     *
     * @todo Unlock()/Revalidate() escape hatch for hot-reload/mod support.
     */
    template<typename TStateId, typename THash = std::hash<TStateId>>
        requires std::equality_comparable<TStateId>
    class StateMachine
    {
    public:
        using TransitionCallback = std::function<void(const TStateId&, const TStateId&)>;

        /// Register a state descriptor. Call before Finalise.
        void AddState(StateDescriptor<TStateId> descriptor)
        {
            // Stub - does not store
        }

        /// Validate the state graph and freeze the machine.
        [[nodiscard]] auto Finalise(const TStateId& initialState) -> Result<void>
        {
            // Stub - always succeeds
            return {};
        }

        /// Enter the initial state. Requires Finalise to have succeeded.
        void Start()
        {
            // Stub
        }

        /// Queue a transition to the given target state. Deferred until ProcessPending.
        void TransitionTo(const TStateId& target)
        {
            // Stub
        }

        /// Execute the pending transition, firing OnExit/OnEnter/observers.
        void ProcessPending()
        {
            // Stub
        }

        [[nodiscard]] auto GetCurrentState() const -> const TStateId&
        {
            return m_currentState;
        }

        [[nodiscard]] auto GetPreviousState() const -> std::optional<TStateId>
        {
            return m_previousState;
        }

        [[nodiscard]] auto IsRunning() const -> bool
        {
            return m_running;
        }

        /// Register a transition observer. Fires after OnEnter with (from, to).
        void OnTransition(TransitionCallback callback)
        {
            // Stub
        }

    private:
        std::unordered_map<TStateId, StateDescriptor<TStateId>, THash> m_states;
        TStateId m_initialState{};
        TStateId m_currentState{};
        std::optional<TStateId> m_previousState;
        std::optional<TStateId> m_pendingTransition;
        std::vector<TransitionCallback> m_observers;
        bool m_finalised = false;
        bool m_running = false;
    };

} // namespace Wayfinder
