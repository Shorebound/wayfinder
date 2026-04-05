#include "ApplicationStateMachine.h"

#include "IApplicationState.h"
#include "core/Assert.h"
#include "core/Log.h"
#include "plugins/IStateUI.h"
#include "plugins/LifecycleHooks.h"

#include <format>
#include <queue>
#include <utility>

namespace Wayfinder
{
    // -- Registration helpers ---------------------------------------------

    void ApplicationStateMachine::AddStateImpl(std::type_index key, std::function<std::unique_ptr<IApplicationState>()> factory, CapabilitySet capabilities)
    {
        WAYFINDER_ASSERT(not m_finalised, "Cannot add states after Finalise()");
        auto [it, inserted] = m_states.emplace(key, StateEntry{.Factory = std::move(factory), .Instance = nullptr, .Capabilities = std::move(capabilities)});
        WAYFINDER_ASSERT(inserted, "Duplicate state registration");
    }

    void ApplicationStateMachine::SetInitialState(std::type_index stateType)
    {
        WAYFINDER_ASSERT(not m_finalised, "Cannot set initial state after Finalise()");
        m_initialState = stateType;
    }

    void ApplicationStateMachine::AddTransitionImpl(std::type_index from, std::type_index to)
    {
        WAYFINDER_ASSERT(not m_finalised, "Cannot add transitions after Finalise()");
        m_flatTransitions[from].insert(to);
    }

    void ApplicationStateMachine::AllowPushImpl(std::type_index stateType)
    {
        WAYFINDER_ASSERT(not m_finalised, "Cannot allow push after Finalise()");
        m_pushableStates.insert(stateType);
    }

    // -- Finalise ---------------------------------------------------------

    auto ApplicationStateMachine::Finalise() -> Result<void>
    {
        WAYFINDER_ASSERT(not m_finalised, "Finalise() already called");

        // Check initial state is registered.
        if (not m_states.contains(m_initialState))
        {
            return MakeError("Initial state not registered");
        }

        // Check all flat transition targets are registered.
        for (const auto& [from, targets] : m_flatTransitions)
        {
            if (not m_states.contains(from))
            {
                return MakeError(std::format("Flat transition source not registered: from unregistered state"));
            }
            for (const auto& target : targets)
            {
                if (not m_states.contains(target))
                {
                    return MakeError(std::format("Flat transition target not registered"));
                }
            }
        }

        // Check all pushable states are registered.
        for (const auto& pushable : m_pushableStates)
        {
            if (not m_states.contains(pushable))
            {
                return MakeError("Pushable state not registered");
            }
        }

        // BFS reachability from initial state.
        // Reachable via flat transitions and push targets.
        std::unordered_set<std::type_index> visited;
        std::queue<std::type_index> frontier;
        frontier.push(m_initialState);
        visited.insert(m_initialState);

        while (not frontier.empty())
        {
            auto current = frontier.front();
            frontier.pop();

            // Flat transitions from current.
            if (auto it = m_flatTransitions.find(current); it != m_flatTransitions.end())
            {
                for (const auto& target : it->second)
                {
                    if (not visited.contains(target))
                    {
                        visited.insert(target);
                        frontier.push(target);
                    }
                }
            }

            // Push targets are reachable from any state.
            for (const auto& pushable : m_pushableStates)
            {
                if (not visited.contains(pushable))
                {
                    visited.insert(pushable);
                    frontier.push(pushable);
                }
            }
        }

        if (visited.size() != m_states.size())
        {
            return MakeError("Unreachable states detected in state graph");
        }

        // Create state instances from factories.
        for (auto& [key, entry] : m_states)
        {
            entry.Instance = entry.Factory();
            WAYFINDER_ASSERT(entry.Instance != nullptr, "State factory returned nullptr");
        }

        m_finalised = true;
        return {};
    }

    // -- Start / Shutdown -------------------------------------------------

    void ApplicationStateMachine::Start(EngineContext& context)
    {
        WAYFINDER_ASSERT(m_finalised, "Start() called before Finalise()");
        WAYFINDER_ASSERT(not m_running, "Start() called while already running");

        m_stack.push_back(m_initialState);
        m_running = true;

        EnterState(context, m_initialState);
    }

    void ApplicationStateMachine::Shutdown(EngineContext& context)
    {
        if (not m_running)
        {
            return;
        }

        // Exit all states in stack order (top to bottom).
        while (not m_stack.empty())
        {
            auto top = m_stack.back();
            ExitState(context, top);
            m_stack.pop_back();
        }

        m_backgroundPolicies.clear();
        m_pending = std::monostate{};
        m_running = false;
    }

    // -- Request operations -----------------------------------------------

    void ApplicationStateMachine::RequestTransitionImpl(std::type_index target)
    {
        WAYFINDER_ASSERT(m_finalised, "RequestTransition called before Finalise()");
        WAYFINDER_ASSERT(m_running, "RequestTransition called while not running");

        if (not std::holds_alternative<std::monostate>(m_pending))
        {
            Log::Warn(LogEngine, "ApplicationStateMachine: overwriting pending operation (last-write-wins)");
        }

        m_pending = FlatTransition{.Target = target};
    }

    void ApplicationStateMachine::RequestPushImpl(std::type_index target)
    {
        WAYFINDER_ASSERT(m_finalised, "RequestPush called before Finalise()");
        WAYFINDER_ASSERT(m_running, "RequestPush called while not running");

        if (not std::holds_alternative<std::monostate>(m_pending))
        {
            Log::Warn(LogEngine, "ApplicationStateMachine: overwriting pending operation (last-write-wins)");
        }

        m_pending = PushTransition{.Target = target};
    }

    void ApplicationStateMachine::RequestPop()
    {
        WAYFINDER_ASSERT(m_finalised, "RequestPop called before Finalise()");
        WAYFINDER_ASSERT(m_running, "RequestPop called while not running");
        WAYFINDER_ASSERT(m_stack.size() > 1, "Cannot pop root state");

        if (not std::holds_alternative<std::monostate>(m_pending))
        {
            Log::Warn(LogEngine, "ApplicationStateMachine: overwriting pending operation (last-write-wins)");
        }

        m_pending = PopTransition{};
    }

    // -- ProcessPending ---------------------------------------------------

    void ApplicationStateMachine::ProcessPending(EngineContext& context)
    {
        auto operation = std::exchange(m_pending, std::monostate{});

        std::visit([&](auto&& op)
        {
            using T = std::decay_t<decltype(op)>;
            if constexpr (std::is_same_v<T, FlatTransition>)
            {
                ExecuteFlatTransition(context, op.Target);
            }
            else if constexpr (std::is_same_v<T, PushTransition>)
            {
                ExecutePush(context, op.Target);
            }
            else if constexpr (std::is_same_v<T, PopTransition>)
            {
                ExecutePop(context);
            }
            // monostate: do nothing.
        }, operation);
    }

    // -- Queries ----------------------------------------------------------

    auto ApplicationStateMachine::GetActiveState() -> IApplicationState*
    {
        if (m_stack.empty())
        {
            return nullptr;
        }
        auto it = m_states.find(m_stack.back());
        return (it != m_states.end()) ? it->second.Instance.get() : nullptr;
    }

    auto ApplicationStateMachine::GetActiveStateType() const -> std::type_index
    {
        WAYFINDER_ASSERT(not m_stack.empty(), "No active state");
        return m_stack.back();
    }

    auto ApplicationStateMachine::GetModalStack() const -> std::span<const std::type_index>
    {
        return m_stack;
    }

    auto ApplicationStateMachine::GetBackgroundPolicy(std::type_index stateType) const -> const EffectiveBackgroundPolicy*
    {
        auto it = m_backgroundPolicies.find(stateType);
        return (it != m_backgroundPolicies.end()) ? &it->second : nullptr;
    }

    auto ApplicationStateMachine::GetStateCapabilities(std::type_index stateType) const -> const CapabilitySet*
    {
        auto it = m_states.find(stateType);
        return (it != m_states.end()) ? &it->second.Capabilities : nullptr;
    }

    auto ApplicationStateMachine::IsRunning() const -> bool
    {
        return m_running;
    }
    auto ApplicationStateMachine::IsFinalised() const -> bool
    {
        return m_finalised;
    }

    // -- Configuration ----------------------------------------------------

    void ApplicationStateMachine::SetLifecycleHooks(const LifecycleHookManifest* hooks)
    {
        m_lifecycleHooks = hooks;
    }

    void ApplicationStateMachine::RegisterStateUI(std::type_index stateType, std::function<std::unique_ptr<IStateUI>()> factory)
    {
        m_stateUIFactories[stateType] = std::move(factory);
    }

    // -- Lifecycle helpers ------------------------------------------------

    void ApplicationStateMachine::EnterState(EngineContext& context, std::type_index stateType)
    {
        auto& entry = m_states.at(stateType);
        auto result = entry.Instance->OnEnter(context);
        if (not result.has_value())
        {
            Log::Warn(LogEngine, "ApplicationStateMachine: OnEnter failed for state '{}': {}", entry.Instance->GetName(), result.error().GetMessage());
        }

        // Create and attach IStateUI if factory exists.
        if (auto uiIt = m_stateUIFactories.find(stateType); uiIt != m_stateUIFactories.end())
        {
            auto ui = uiIt->second();
            if (ui)
            {
                auto uiResult = ui->OnAttach(context);
                if (not uiResult.has_value())
                {
                    Log::Warn(LogEngine, "ApplicationStateMachine: IStateUI OnAttach failed for '{}'", ui->GetName());
                }
                m_stateUIs[stateType] = std::move(ui);
            }
        }

        // Fire lifecycle hooks.
        if (m_lifecycleHooks)
        {
            m_lifecycleHooks->FireStateEnter(context, stateType);
        }
    }

    void ApplicationStateMachine::ExitState(EngineContext& context, std::type_index stateType)
    {
        // Detach IStateUI if exists.
        if (auto uiIt = m_stateUIs.find(stateType); uiIt != m_stateUIs.end())
        {
            auto detachResult = uiIt->second->OnDetach(context);
            if (not detachResult.has_value())
            {
                Log::Warn(LogEngine, "ApplicationStateMachine: IStateUI OnDetach failed");
            }
            m_stateUIs.erase(uiIt);
        }

        auto& entry = m_states.at(stateType);
        auto result = entry.Instance->OnExit(context);
        if (not result.has_value())
        {
            Log::Warn(LogEngine, "ApplicationStateMachine: OnExit failed for state '{}': {}", entry.Instance->GetName(), result.error().GetMessage());
        }

        // Fire lifecycle hooks.
        if (m_lifecycleHooks)
        {
            m_lifecycleHooks->FireStateExit(context, stateType);
        }
    }

    void ApplicationStateMachine::SuspendState(EngineContext& context, std::type_index stateType)
    {
        auto& entry = m_states.at(stateType);
        entry.Instance->OnSuspend(context);

        if (auto uiIt = m_stateUIs.find(stateType); uiIt != m_stateUIs.end())
        {
            uiIt->second->OnSuspend(context);
        }
    }

    void ApplicationStateMachine::ResumeState(EngineContext& context, std::type_index stateType)
    {
        auto& entry = m_states.at(stateType);
        entry.Instance->OnResume(context);

        if (auto uiIt = m_stateUIs.find(stateType); uiIt != m_stateUIs.end())
        {
            uiIt->second->OnResume(context);
        }
    }

    // -- Transition execution ---------------------------------------------

    void ApplicationStateMachine::ExecuteFlatTransition(EngineContext& context, std::type_index target)
    {
        WAYFINDER_ASSERT(not m_stack.empty(), "Flat transition with empty stack");

        auto current = m_stack.back();
        ExitState(context, current);
        m_stack.back() = target;
        EnterState(context, target);
    }

    void ApplicationStateMachine::ExecutePush(EngineContext& context, std::type_index target)
    {
        WAYFINDER_ASSERT(not m_stack.empty(), "Push transition with empty stack");

        auto current = m_stack.back();

        // Compute negotiated background policy.
        auto& currentEntry = m_states.at(current);
        auto& targetEntry = m_states.at(target);
        auto backgroundPrefs = currentEntry.Instance->GetBackgroundPreferences();
        auto suspensionPolicy = targetEntry.Instance->GetSuspensionPolicy();
        m_backgroundPolicies[current] = ComputeBackgroundPolicy(backgroundPrefs, suspensionPolicy);

        SuspendState(context, current);
        m_stack.push_back(target);
        EnterState(context, target);
    }

    void ApplicationStateMachine::ExecutePop(EngineContext& context)
    {
        WAYFINDER_ASSERT(m_stack.size() > 1, "Cannot pop root state");

        auto top = m_stack.back();
        ExitState(context, top);
        m_stack.pop_back();

        // Erase cached background policy for the popped state's predecessor
        // (if this was a multilevel push, the policy for the now-active state is no longer needed).
        m_backgroundPolicies.erase(m_stack.back());

        ResumeState(context, m_stack.back());
    }

} // namespace Wayfinder
