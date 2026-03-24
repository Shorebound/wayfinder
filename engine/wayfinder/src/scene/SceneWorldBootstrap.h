#pragma once

#include "wayfinder_exports.h"

namespace flecs
{
    struct world;
}

namespace Wayfinder::Plugins
{
    class PluginRegistry;
}

namespace Wayfinder
{
    /**
     * @brief Registers the default transform and camera scene plugins (single source of truth for
     *        headless bootstrap and sandbox game roots).
     */
    void PopulateDefaultScenePlugins(Plugins::PluginRegistry& registry);

    /**
     * @brief Headless world bootstrap: applies the default scene plugins (transform propagation,
     *        active camera extraction) that a typical game root plugin would add.
     *
     * Call after \ref RuntimeComponentRegistry::RegisterComponents when using scene JSON, and
     * after \ref Scene::RegisterCoreComponents for minimal worlds, before \c world.progress().
     *
     * Uses a short-lived \ref PluginRegistry; \ref Plugin::OnStartup / \ref Plugin::OnShutdown are
     * not invoked. For full lifecycle use a persistent \ref PluginRegistry (as \ref Application does).
     */
    struct WAYFINDER_API SceneWorldBootstrap
    {
        static void RegisterDefaultScenePlugins(flecs::world& world);
    };

} // namespace Wayfinder
