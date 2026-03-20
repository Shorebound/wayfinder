#include "GameStateMachine.h"
#include "GameState.h"
#include "InternedString.h"
#include "Log.h"
#include "ModuleRegistry.h"

#include <flecs.h>

namespace Wayfinder
{
    void GameStateMachine::Configure(flecs::world& world, const ModuleRegistry* moduleRegistry)
    {
        m_world = &world;
        m_moduleRegistry = moduleRegistry;
        m_conditionedSystems.clear();
        m_runConditionsDirty = false;
    }

    void GameStateMachine::Setup()
    {
        BindConditionedSystems();

        if (m_moduleRegistry)
        {
            const auto& initialState = m_moduleRegistry->GetInitialState();
            if (!initialState.empty())
            {
                TransitionTo(initialState);
            }
            else
            {
                EvaluateRunConditions();
            }
        }
    }

    void GameStateMachine::Update()
    {
        if (m_runConditionsDirty)
        {
            EvaluateRunConditions();
            m_runConditionsDirty = false;
        }
    }

    void GameStateMachine::TransitionTo(const std::string& stateName)
    {
        WAYFINDER_ASSERT(m_world, "TransitionTo() called before Configure()");

        // Early-out if already in the requested state (avoids interning).
        {
            const ActiveGameState& state = m_world->get<ActiveGameState>();
            if (state.Current.GetString() == stateName)
                return;
        }

        const ModuleRegistry::StateDescriptor* targetDesc = nullptr;
        const ModuleRegistry::StateDescriptor* exitDesc = nullptr;
        if (m_moduleRegistry)
        {
            const ActiveGameState& state = m_world->get<ActiveGameState>();
            for (const auto& desc : m_moduleRegistry->GetStateDescriptors())
            {
                if (desc.Name == stateName)
                    targetDesc = &desc;
                if (!state.Current.IsEmpty() && desc.Name == state.Current.GetString())
                    exitDesc = &desc;
            }

            if (!targetDesc)
            {
                WAYFINDER_WARNING(LogGame, "TransitionTo: '{}' is not a registered state", stateName);
                return;
            }
        }

        const auto internedName = InternedString::Intern(stateName);
        ActiveGameState& state = m_world->get_mut<ActiveGameState>();
        const InternedString oldState = state.Current;

        // Call OnExit for the outgoing state
        if (exitDesc && exitDesc->OnExit)
            exitDesc->OnExit(*m_world);

        // Update the singleton
        state.Previous = oldState;
        state.Current = internedName;

        // Call OnEnter for the incoming state
        if (targetDesc && targetDesc->OnEnter)
            targetDesc->OnEnter(*m_world);

        WAYFINDER_INFO(LogGame, "State transition: '{}' -> '{}'", oldState.GetString(), stateName);

        m_runConditionsDirty = true;
    }

    std::string_view GameStateMachine::GetCurrentState() const
    {
        WAYFINDER_ASSERT(m_world, "GetCurrentState() called before Configure()");
        const ActiveGameState& state = m_world->get<ActiveGameState>();
        return state.Current.GetString();
    }

    void GameStateMachine::BindConditionedSystems()
    {
        if (!m_moduleRegistry)
            return;

        for (const auto& desc : m_moduleRegistry->GetSystems())
        {
            if (!desc.Condition)
                continue;

            flecs::entity sys = m_world->lookup(desc.Name.c_str());
            if (!sys.is_valid())
            {
                WAYFINDER_WARNING(LogGame,
                    "Conditioned system '{}' not found in world. "
                    "Ensure the flecs system name matches the descriptor name.",
                    desc.Name);
                continue;
            }

            const bool initiallyEnabled = desc.Condition(*m_world);
            if (!initiallyEnabled)
                sys.disable();

            m_conditionedSystems.push_back({sys, desc.Condition, initiallyEnabled});
        }
    }

    void GameStateMachine::EvaluateRunConditions()
    {
        for (auto& cs : m_conditionedSystems)
        {
            const bool shouldRun = cs.Condition(*m_world);
            if (shouldRun != cs.Enabled)
            {
                if (shouldRun)
                    cs.SystemEntity.enable();
                else
                    cs.SystemEntity.disable();
                cs.Enabled = shouldRun;
            }
        }
    }

} // namespace Wayfinder
