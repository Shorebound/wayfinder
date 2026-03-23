#include "CameraPlugin.h"

#include "maths/Maths.h"
#include "plugins/PluginRegistry.h"
#include "scene/Components.h"

#include "ecs/Flecs.h"

namespace Wayfinder
{
    void CameraPlugin::Build(PluginRegistry& registry)
    {
        registry.RegisterSystem("ExtractActiveCamera", [](flecs::world& world)
        {
            world.system<>("ExtractActiveCamera")
                .kind(flecs::OnUpdate)
                .run([&world](flecs::iter&)
            {
                ActiveCameraStateComponent activeCamera;

                world.each([&](flecs::entity entityHandle, const TransformComponent& transform, const CameraComponent& camera)
                {
                    if (activeCamera.IsValid || !camera.Primary)
                    {
                        return;
                    }

                    activeCamera.IsValid = true;
                    activeCamera.FieldOfView = camera.FieldOfView;
                    activeCamera.Projection = camera.Projection;

                    if (entityHandle.has<WorldTransformComponent>())
                    {
                        const auto& worldTransform = entityHandle.get<WorldTransformComponent>();
                        activeCamera.Position = worldTransform.Position;
                        activeCamera.Target = camera.Target;
                        activeCamera.Up = Maths::Normalize(Maths::TransformDirection(worldTransform.LocalToWorld, camera.Up));
                    }
                    else
                    {
                        activeCamera.Position = transform.Local.Position;
                        activeCamera.Target = camera.Target;
                        activeCamera.Up = camera.Up;
                    }
                });

                world.set<ActiveCameraStateComponent>(activeCamera);
            });
        });
    }

} // namespace Wayfinder
