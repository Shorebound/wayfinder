#pragma once

#include "gameplay/Capability.h"

#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wayfinder
{
    class IApplicationState;
    class IStateUI;

    /**
     * @brief Processed output from state registration, stored in AppDescriptor.
     *
     * Consumed by Application to build the ApplicationStateMachine.
     * Produced by AppBuilder::Finalise() from accumulated AddState/AddTransition/etc. calls.
     */
    struct StateManifest
    {
        struct StateEntry
        {
            std::type_index Type;
            std::function<std::unique_ptr<IApplicationState>()> Factory;
            CapabilitySet Capabilities;
        };

        std::vector<StateEntry> States;
        std::type_index InitialState{typeid(void)};
        std::unordered_map<std::type_index, std::unordered_set<std::type_index>> FlatTransitions;
        std::unordered_set<std::type_index> PushableStates;
        std::unordered_map<std::type_index, std::function<std::unique_ptr<IStateUI>()>> StateUIFactories;
    };

} // namespace Wayfinder
