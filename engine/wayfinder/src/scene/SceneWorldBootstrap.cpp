#include "SceneWorldBootstrap.h"

#include "app/EngineConfig.h"
#include "ecs/Flecs.h"
#include "plugins/PluginRegistry.h"
#include "project/ProjectDescriptor.h"
#include "scene/plugins/CameraPlugin.h"
#include "scene/plugins/TransformPlugin.h"

namespace Wayfinder
{
    void SceneWorldBootstrap::RegisterDefaultScenePlugins(flecs::world& world)
    {
        ProjectDescriptor project{};
        project.Name = "HeadlessTest";
        const EngineConfig config = EngineConfig::LoadDefaults();
        Plugins::PluginRegistry registry(project, config);
        registry.AddPlugin<TransformPlugin>();
        registry.AddPlugin<CameraPlugin>();
        registry.ApplyComponentRegisterFns(world);
        registry.ApplyToWorld(world);
    }

} // namespace Wayfinder
