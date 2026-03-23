#include "GameStateMachine.h"
#include "GameState.h"
#include "core/Assert.h"
#include "core/InternedString.h"
#include "core/Log.h"
#include "plugins/PluginRegistry.h"

#include "ecs/Flecs.h"

namespace Wayfinder
{
    void GameStateMachine::Configure(flecs::world& world, const PluginRegistry& pluginRegistry)
    {
        m_world = &world;
        m_pluginRegistry = &pluginRegistry;
        m_conditionedSystems.clear();
        m_runConditionsDirty = false;
    }

    void GameStateMachine::Setup()
    {
        BindConditionedSystems();

        const auto& initialState = m_pluginRegistry->GetInitialState();
        if (!initialState.empty())
        {
            TransitionTo(initialState);
        }
        else
        {
            EvaluateRunConditions();
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
            const auto& state = m_world->get<ActiveGameState>();
            if (state.Current.GetString() == stateName)
            {
                return;
            }
        }

        const PluginRegistry::StateDescriptor* targetDesc = nullptr;
        const PluginRegistry::StateDescriptor* exitDesc = nullptr;
        {
            const auto& state = m_world->get<ActiveGameState>();
            for (const auto& desc : m_pluginRegistry->GetStateDescriptors())
            {
                if (desc.Name == stateName)
                {
                    targetDesc = &desc;
                }
                if (!state.Current.IsEmpty() && desc.Name == state.Current.GetString())
                {
                    exitDesc = &desc;
                }
            }

            if (!targetDesc)
            {
                WAYFINDER_WARNING(LogGame, "TransitionTo: '{}' is not a registered state", stateName);
                return;
            }
        }

        const auto internedName = InternedString::Intern(stateName);
        auto& state = m_world->get_mut<ActiveGameState>();
        const InternedString oldState = state.Current;

        // Call OnExit for the outgoing state
        if (exitDesc && exitDesc->OnExit)
        {
            exitDesc->OnExit(*m_world);
        }

        // Update the singleton
        state.Previous = oldState;
        state.Current = internedName;

        // Call OnEnter for the incoming state
        if (targetDesc && targetDesc->OnEnter)
        {
            targetDesc->OnEnter(*m_world);
        }

        WAYFINDER_INFO(LogGame, "State transition: '{}' -> '{}'", oldState.GetString(), stateName);

        m_runConditionsDirty = true;
    }

    std::string_view GameStateMachine::GetCurrentState() const
    {
        WAYFINDER_ASSERT(m_world, "GetCurrentState() called before Configure()");
        const auto& state = m_world->get<ActiveGameState>();
        return state.Current.GetString();
    }

    void GameStateMachine::BindConditionedSystems()
    {
        WAYFINDER_ASSERT(m_pluginRegistry, "BindConditionedSystems() called before Configure()");

        for (const auto& desc : m_pluginRegistry->GetSystems())
        {
            if (!desc.Condition)
            {
                continue;
            }

            const flecs::entity sys = m_world->lookup(desc.Name.c_str());
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
            {
                sys.disable();
            }

            m_conditionedSystems.push_back({.SystemEntity = sys, .Condition = desc.Condition, .Enabled = initiallyEnabled});
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
                {
                    cs.SystemEntity.enable();
                }
                else
                {
                    cs.SystemEntity.disable();
                }
                cs.Enabled = shouldRun;
            }
        }
    }

} // namespace Wayfinder
