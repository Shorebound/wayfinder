#include "StateRegistrar.h"
#include "core/Log.h"
#include "core/Result.h"

namespace Wayfinder::Plugins
{
    Result<void> StateRegistrar::Register(Descriptor descriptor)
    {
        for (const auto& existing : m_descriptors)
        {
            if (existing.Name == descriptor.Name)
            {
                return MakeError(std::format("StateRegistrar: duplicate state name '{}'", descriptor.Name));
            }
        }

        Log::Info(LogEngine, "StateRegistrar: registered state '{}'", descriptor.Name);
        m_descriptors.push_back(std::move(descriptor));
        return {};
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

        Log::Error(LogEngine, "StateRegistrar: SetInitial '{}' - state not registered; ignoring", stateName);
    }

} // namespace Wayfinder::Plugins
