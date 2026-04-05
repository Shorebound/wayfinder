#pragma once

#include "OrchestrationTypes.h"
#include "core/Result.h"
#include "gameplay/Capability.h"

#include <concepts>
#include <functional>
#include <memory>
#include <span>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wayfinder
{
    class EngineContext;
    class IApplicationState;
    class IStateUI;
    struct LifecycleHookManifest;

    /**
     * @brief Manages application state lifecycle with flat transitions, push/pop modal stack,
     *        deferred execution at frame boundaries, and build-time graph validation.
     *
     * States are keyed by std::type_index, owned via unique_ptr. The state graph is validated
     * at Finalise() time (BFS reachability). Transitions are deferred until ProcessPending()
     * is called at the frame boundary.
     *
     * Push/pop negotiations compute an effective BackgroundMode from both the suspended state's
     * background preferences and the pushing state's suspension policy.
     */
    class ApplicationStateMachine
    {
    public:
        ApplicationStateMachine() = default;
        ~ApplicationStateMachine() = default;

        ApplicationStateMachine(const ApplicationStateMachine&) = delete;
        auto operator=(const ApplicationStateMachine&) -> ApplicationStateMachine& = delete;
        ApplicationStateMachine(ApplicationStateMachine&&) = default;
        auto operator=(ApplicationStateMachine&&) -> ApplicationStateMachine& = default;

        // -- Registration (before Finalise) --

        /// Register a state type with optional capabilities.
        template<std::derived_from<IApplicationState> T>
        void AddState(CapabilitySet capabilities = {})
        {
            auto key = std::type_index(typeid(T));
            AddStateImpl(key, []()
            {
                return std::unique_ptr<IApplicationState>(std::make_unique<T>());
            }, std::move(capabilities));
        }

        /// Mark which state is entered first after Start().
        void SetInitialState(std::type_index stateType);

        /// Mark which state is entered first after Start().
        template<std::derived_from<IApplicationState> T>
        void SetInitialState()
        {
            SetInitialState(std::type_index(typeid(T)));
        }

        /// Declare a valid flat transition between two state types.
        template<std::derived_from<IApplicationState> TFrom, std::derived_from<IApplicationState> TTo>
        void AddTransition()
        {
            AddTransitionImpl(std::type_index(typeid(TFrom)), std::type_index(typeid(TTo)));
        }

        /// Declare a state as pushable (any state can push to it).
        template<std::derived_from<IApplicationState> T>
        void AllowPush()
        {
            AllowPushImpl(std::type_index(typeid(T)));
        }

        /// Validate the state graph and create state instances. Must be called before Start().
        [[nodiscard]] auto Finalise() -> Result<void>;

        // -- Runtime (after Finalise + Start) --

        /// Enter the initial state. Asserts finalised and not already running.
        void Start(EngineContext& context);

        /// Queue a flat transition to the target state type.
        template<std::derived_from<IApplicationState> T>
        void RequestTransition()
        {
            RequestTransitionImpl(std::type_index(typeid(T)));
        }

        /// Queue a push transition to the target state type.
        template<std::derived_from<IApplicationState> T>
        void RequestPush()
        {
            RequestPushImpl(std::type_index(typeid(T)));
        }

        /// Queue a pop transition (resume the state beneath).
        void RequestPop();

        /// Execute the pending operation at the frame boundary.
        void ProcessPending(EngineContext& context);

        /// Exit all states in stack order and reset.
        void Shutdown(EngineContext& context);

        // -- Queries --

        /// Return the top-of-stack (active) state, or nullptr if not running.
        [[nodiscard]] auto GetActiveState() -> IApplicationState*;

        /// Return the type_index of the active state.
        [[nodiscard]] auto GetActiveStateType() const -> std::type_index;

        /// Read-only view of the modal stack (back = active).
        [[nodiscard]] auto GetModalStack() const -> std::span<const std::type_index>;

        /// Return cached background policy for a suspended state, or nullptr.
        [[nodiscard]] auto GetBackgroundPolicy(std::type_index stateType) const -> const BackgroundMode*;

        /// Return the capabilities declared for a state type.
        [[nodiscard]] auto GetStateCapabilities(std::type_index stateType) const -> const CapabilitySet*;

        [[nodiscard]] auto IsRunning() const -> bool;
        [[nodiscard]] auto IsFinalised() const -> bool;

        // -- Configuration --

        /// Set the lifecycle hook manifest (non-owning pointer).
        void SetLifecycleHooks(const LifecycleHookManifest* hooks);

        /// Register an IStateUI factory for a specific state type.
        void RegisterStateUI(std::type_index stateType, std::function<std::unique_ptr<IStateUI>()> factory);

    private:
        // -- Registration helpers --
        void AddStateImpl(std::type_index key, std::function<std::unique_ptr<IApplicationState>()> factory, CapabilitySet capabilities);
        void AddTransitionImpl(std::type_index from, std::type_index to);
        void AllowPushImpl(std::type_index stateType);
        void RequestTransitionImpl(std::type_index target);
        void RequestPushImpl(std::type_index target);

        // -- Lifecycle helpers --
        void EnterState(EngineContext& context, std::type_index stateType);
        void ExitState(EngineContext& context, std::type_index stateType);
        void SuspendState(EngineContext& context, std::type_index stateType);
        void ResumeState(EngineContext& context, std::type_index stateType);

        // -- Transition execution --
        void ExecuteFlatTransition(EngineContext& context, std::type_index target);
        void ExecutePush(EngineContext& context, std::type_index target);
        void ExecutePop(EngineContext& context);

        // -- State storage --
        struct StateEntry
        {
            std::function<std::unique_ptr<IApplicationState>()> Factory;
            std::unique_ptr<IApplicationState> Instance;
            CapabilitySet Capabilities;
        };

        std::unordered_map<std::type_index, StateEntry> m_states;
        std::unordered_map<std::type_index, std::unordered_set<std::type_index>> m_flatTransitions;
        std::unordered_set<std::type_index> m_pushableStates;

        std::vector<std::type_index> m_stack;
        PendingOperation m_pending;
        std::type_index m_initialState{typeid(void)};

        std::unordered_map<std::type_index, BackgroundMode> m_backgroundPolicies;
        std::unordered_map<std::type_index, std::unique_ptr<IStateUI>> m_stateUIs;
        std::unordered_map<std::type_index, std::function<std::unique_ptr<IStateUI>()>> m_stateUIFactories;

        const LifecycleHookManifest* m_lifecycleHooks = nullptr;
        bool m_finalised = false;
        bool m_running = false;
    };

} // namespace Wayfinder
