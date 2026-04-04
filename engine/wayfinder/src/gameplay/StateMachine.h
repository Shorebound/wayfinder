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
            assert(not m_finalised);
            m_states.emplace(descriptor.Id, std::move(descriptor));
        }

        /// Validate the state graph and freeze the machine.
        /// Checks: initial state exists, no dangling transition targets,
        /// all registered states are reachable from the initial state via BFS.
        [[nodiscard]] auto Finalise(const TStateId& initialState) -> Result<void>
        {
            if (not m_states.contains(initialState))
            {
                return MakeError("Initial state not registered");
            }

            for (const auto& [id, descriptor] : m_states)
            {
                for (const auto& target : descriptor.AllowedTransitions)
                {
                    if (not m_states.contains(target))
                    {
                        return MakeError("Dangling transition target from state");
                    }
                }
            }

            std::unordered_set<TStateId, THash> visited;
            std::queue<TStateId> frontier;
            frontier.push(initialState);
            visited.insert(initialState);

            while (not frontier.empty())
            {
                auto current = frontier.front();
                frontier.pop();

                const auto& desc = m_states.at(current);
                for (const auto& target : desc.AllowedTransitions)
                {
                    if (not visited.contains(target))
                    {
                        visited.insert(target);
                        frontier.push(target);
                    }
                }
            }

            if (visited.size() != m_states.size())
            {
                return MakeError("Unreachable states detected in state graph");
            }

            m_initialState = initialState;
            m_finalised = true;
            return {};
        }

        /// Enter the initial state. Requires Finalise to have succeeded.
        void Start()
        {
            assert(m_finalised);
            assert(not m_running);

            m_currentState = m_initialState;
            m_running = true;

            const auto& descriptor = m_states.at(m_currentState);
            if (descriptor.OnEnter)
            {
                descriptor.OnEnter();
            }
        }

        /// Queue a transition to the given target state. Deferred until ProcessPending.
        void TransitionTo(const TStateId& target)
        {
            assert(m_running);

            [[maybe_unused]] const auto& descriptor = m_states.at(m_currentState);
            [[maybe_unused]] bool allowed = false;
            for (const auto& t : descriptor.AllowedTransitions)
            {
                if (t == target)
                {
                    allowed = true;
                    break;
                }
            }
            assert(allowed);

            m_pendingTransition = target;
        }

        /// Execute the pending transition, firing OnExit/OnEnter/observers.
        void ProcessPending()
        {
            if (not m_pendingTransition.has_value())
            {
                return;
            }

            const auto target = m_pendingTransition.value();
            m_pendingTransition.reset();

            const auto& currentDescriptor = m_states.at(m_currentState);
            if (currentDescriptor.OnExit)
            {
                currentDescriptor.OnExit();
            }

            const auto previous = m_currentState;
            m_previousState = m_currentState;
            m_currentState = target;

            const auto& newDescriptor = m_states.at(m_currentState);
            if (newDescriptor.OnEnter)
            {
                newDescriptor.OnEnter();
            }

            for (const auto& observer : m_observers)
            {
                observer(previous, m_currentState);
            }
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
            m_observers.push_back(std::move(callback));
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
