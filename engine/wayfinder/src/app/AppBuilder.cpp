#include "app/AppBuilder.h"

#include "core/TopologicalSort.h"
#include "core/ValidationResult.h"

#include <format>

namespace Wayfinder
{

    auto AppBuilder::Finalise() -> Result<AppDescriptor>
    {
        WAYFINDER_ASSERT(not m_finalised, "Finalise() already called");

        const size_t pluginCount = m_plugins.size();

        // Step 1: Build plugin dependency graph
        std::vector<std::vector<size_t>> adjacency(pluginCount);
        ValidationResult validation;

        for (size_t i = 0; i < pluginCount; ++i)
        {
            for (const auto& dep : m_plugins[i].DependsOn)
            {
                auto it = m_pluginTypeToIndex.find(dep);
                if (it == m_pluginTypeToIndex.end())
                {
                    validation.AddError("PluginDependency", std::format("Plugin '{}' depends on unregistered plugin type '{}'", m_plugins[i].Name.GetString(), dep.name()));
                    continue;
                }
                // dep must build before i: edge from dep -> i
                adjacency[it->second].push_back(i);
            }
        }

        if (validation.HasErrors())
        {
            return std::unexpected(validation.ToError());
        }

        // Step 2: Topological sort
        auto sortResult = TopologicalSort(pluginCount, adjacency, [this](size_t i)
        {
            return std::string(m_plugins[i].Name.GetString());
        });

        if (sortResult.HasCycle)
        {
            return MakeError(std::format("Plugin dependency cycle detected: {}", sortResult.CyclePath));
        }

        // Step 3: Call Build() in topological order
        for (size_t idx : sortResult.Order)
        {
            m_plugins[idx].Instance->Build(*this);
        }

        // Step 4: Finalise registrars and collect outputs
        AppDescriptor descriptor;

        // Finalise LifecycleHookRegistrar if present
        if (auto it = m_registrars.find(std::type_index(typeid(LifecycleHookRegistrar))); it != m_registrars.end())
        {
            auto& hookRegistrar = static_cast<LifecycleHookRegistrar&>(*it->second);
            auto hookResult = hookRegistrar.Finalise();
            if (not hookResult)
            {
                return std::unexpected(hookResult.error());
            }
            descriptor.AddOutput(std::move(*hookResult));
        }

        // Note: SubsystemRegistry registrars are NOT finalised here.
        // They require EngineContext (which doesn't exist until Application startup).
        // Application extracts them via TakeRegistrar<>() and finalises separately.

        m_finalised = true;
        return descriptor;
    }

} // namespace Wayfinder
