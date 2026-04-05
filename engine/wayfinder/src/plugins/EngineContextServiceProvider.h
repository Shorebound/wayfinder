#pragma once

#include "app/EngineContext.h"
#include "plugins/ServiceProvider.h"

namespace Wayfinder
{
    /**
     * @brief ServiceProvider adapter that delegates to EngineContext app subsystems.
     *
     * Used by live (non-headless) code paths where a ServiceProvider is required
     * but services are accessed through the engine context.
     */
    struct EngineContextServiceProvider
    {
        EngineContext& Context;

        template<typename T>
        [[nodiscard]] auto Get() -> T&
        {
            return Context.GetAppSubsystem<T>();
        }

        template<typename T>
        [[nodiscard]] auto TryGet() -> T*
        {
            return Context.TryGetAppSubsystem<T>();
        }
    };

    static_assert(ServiceProvider<EngineContextServiceProvider>);

} // namespace Wayfinder
