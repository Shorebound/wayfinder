#pragma once

#include "core/Result.h"
#include "plugins/IRegistrar.h"

#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{
    class EngineContext;

    /// Immutable output from LifecycleHookRegistrar::Finalise().
    /// Stored in AppDescriptor, consumed by Application at lifecycle points.
    struct LifecycleHookManifest
    {
        std::vector<std::function<void(EngineContext&)>> OnAppReady;
        std::vector<std::function<void()>> OnShutdown;
        std::unordered_map<std::type_index, std::vector<std::function<void(EngineContext&)>>> OnStateEnter;
        std::unordered_map<std::type_index, std::vector<std::function<void(EngineContext&)>>> OnStateExit;

        /// Fire all OnStateEnter hooks for a specific state type.
        template<typename TState>
        void FireStateEnter(EngineContext& context) const
        {
            auto it = OnStateEnter.find(std::type_index(typeid(TState)));
            if (it != OnStateEnter.end())
            {
                for (const auto& hook : it->second)
                {
                    hook(context);
                }
            }
        }

        /// Fire all OnStateEnter hooks by type_index (runtime dispatch).
        void FireStateEnter(EngineContext& context, std::type_index stateType) const
        {
            auto it = OnStateEnter.find(stateType);
            if (it != OnStateEnter.end())
            {
                for (const auto& hook : it->second)
                {
                    hook(context);
                }
            }
        }

        /// Fire all OnStateExit hooks for a specific state type.
        template<typename TState>
        void FireStateExit(EngineContext& context) const
        {
            auto it = OnStateExit.find(std::type_index(typeid(TState)));
            if (it != OnStateExit.end())
            {
                for (const auto& hook : it->second)
                {
                    hook(context);
                }
            }
        }

        /// Fire all OnStateExit hooks by type_index (runtime dispatch).
        void FireStateExit(EngineContext& context, std::type_index stateType) const
        {
            auto it = OnStateExit.find(stateType);
            if (it != OnStateExit.end())
            {
                for (const auto& hook : it->second)
                {
                    hook(context);
                }
            }
        }
    };

    /// Registrar for lifecycle hooks. Built during plugin Build() phase.
    class LifecycleHookRegistrar : public IRegistrar
    {
    public:
        void OnAppReady(std::function<void(EngineContext&)> callback)
        {
            m_onAppReady.push_back(std::move(callback));
        }

        void OnShutdown(std::function<void()> callback)
        {
            m_onShutdown.push_back(std::move(callback));
        }

        template<typename TState>
        void OnStateEnter(std::function<void(EngineContext&)> callback)
        {
            m_onStateEnter[std::type_index(typeid(TState))].push_back(std::move(callback));
        }

        template<typename TState>
        void OnStateExit(std::function<void(EngineContext&)> callback)
        {
            m_onStateExit[std::type_index(typeid(TState))].push_back(std::move(callback));
        }

        /// Produce the final manifest. Moves all stored hooks out.
        [[nodiscard]] auto Finalise() -> Result<LifecycleHookManifest>
        {
            return LifecycleHookManifest{
                .OnAppReady = std::move(m_onAppReady),
                .OnShutdown = std::move(m_onShutdown),
                .OnStateEnter = std::move(m_onStateEnter),
                .OnStateExit = std::move(m_onStateExit),
            };
        }

    private:
        std::vector<std::function<void(EngineContext&)>> m_onAppReady;
        std::vector<std::function<void()>> m_onShutdown;
        std::unordered_map<std::type_index, std::vector<std::function<void(EngineContext&)>>> m_onStateEnter;
        std::unordered_map<std::type_index, std::vector<std::function<void(EngineContext&)>>> m_onStateExit;
    };

} // namespace Wayfinder
