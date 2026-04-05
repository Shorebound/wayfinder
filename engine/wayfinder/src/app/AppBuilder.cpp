#include "app/AppBuilder.h"

#include "core/TopologicalSort.h"
#include "core/ValidationResult.h"

#include <format>

namespace Wayfinder
{

    void AppBuilder::SetProjectPaths(const std::filesystem::path& configDir, const std::filesystem::path& savedDir)
    {
        m_configDir = configDir;
        m_savedDir = savedDir;
    }

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
        m_building = true;
        for (const size_t idx : sortResult.Order)
        {
            m_plugins[idx].Instance->Build(*this);
        }
        m_building = false;

        // Check for config errors accumulated during Build()
        if (not m_configErrors.empty())
        {
            ValidationResult configValidation;
            for (auto& err : m_configErrors)
            {
                configValidation.AddError("Config", std::move(err));
            }
            return std::unexpected(configValidation.ToError());
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

        // Produce StateManifest from accumulated state registrations.
        if (not m_stateEntries.empty())
        {
            StateManifest stateManifest;
            stateManifest.InitialState = m_initialState;
            stateManifest.FlatTransitions = std::move(m_flatTransitions);
            stateManifest.PushableStates = std::move(m_pushableStates);
            stateManifest.StateUIFactories = std::move(m_stateUIFactories);
            for (auto& [type, entry] : m_stateEntries)
            {
                stateManifest.States.push_back(std::move(entry));
            }
            descriptor.AddOutput(std::move(stateManifest));
        }

        // Produce OverlayManifest from accumulated overlay registrations.
        if (not m_overlayEntries.empty())
        {
            OverlayManifest overlayManifest;
            overlayManifest.Overlays = std::move(m_overlayEntries);
            descriptor.AddOutput(std::move(overlayManifest));
        }

        m_finalised = true;
        return descriptor;
    }

} // namespace Wayfinder
