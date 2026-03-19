#include "StateRegistrar.h"
#include "../Log.h"

namespace Wayfinder
{
    void StateRegistrar::Register(Descriptor descriptor)
    {
        for (const auto& existing : m_descriptors)
        {
            if (existing.Name == descriptor.Name)
            {
                WAYFINDER_ERROR(LogEngine, "StateRegistrar: duplicate state name '{}' — registration rejected", descriptor.Name);
                return;
            }
        }

        WAYFINDER_INFO(LogEngine, "StateRegistrar: registered state '{}'", descriptor.Name);
        m_descriptors.push_back(std::move(descriptor));
    }

    void StateRegistrar::SetInitial(std::string stateName)
    {
        m_initialState = std::move(stateName);
    }

} // namespace Wayfinder
