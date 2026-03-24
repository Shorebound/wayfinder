#include "SceneWorldBootstrap.h"

#include "app/EngineConfig.h"
#include "ecs/Flecs.h"
#include "plugins/PluginRegistry.h"
#include "project/ProjectDescriptor.h"
#include "scene/plugins/CameraPlugin.h"
#include "scene/plugins/TransformPlugin.h"

namespace Wayfinder
{
    void PopulateDefaultScenePlugins(Plugins::PluginRegistry& registry)
    {
        registry.AddPlugin<TransformPlugin>();
        registry.AddPlugin<CameraPlugin>();
    }

    void SceneWorldBootstrap::RegisterDefaultScenePlugins(flecs::world& world)
    {
        ProjectDescriptor project{};
        project.Name = "HeadlessTest";
        const EngineConfig config = EngineConfig::LoadDefaults();
        Plugins::PluginRegistry registry(project, config);
        PopulateDefaultScenePlugins(registry);
        registry.ApplyComponentRegisterFns(world);
        registry.ApplyToWorld(world);
    }

} // namespace Wayfinder
