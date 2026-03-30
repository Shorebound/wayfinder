#include "StateRegistrar.h"
#include "core/Log.h"

namespace Wayfinder::Plugins
{
    void StateRegistrar::Register(Descriptor descriptor)
    {
        for (const auto& existing : m_descriptors)
        {
            if (existing.Name == descriptor.Name)
            {
                Log::Error(LogEngine, "StateRegistrar: duplicate state name '{}' — registration rejected", descriptor.Name);
                return;
            }
        }

        Log::Info(LogEngine, "StateRegistrar: registered state '{}'", descriptor.Name);
        m_descriptors.push_back(std::move(descriptor));
    }

    void StateRegistrar::SetInitial(std::string stateName)
    {
        for (const auto& desc : m_descriptors)
        {
            if (desc.Name == stateName)
            {
                m_initialState = std::move(stateName);
                return;
            }
        }

        Log::Error(LogEngine, "StateRegistrar: SetInitial '{}' — state not registered; ignoring", stateName);
    }

} // namespace Wayfinder::Plugins
