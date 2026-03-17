#include "SceneModuleRegistry.h"

#include "Components.h"

#include <array>

namespace
{
    template <typename T>
    void RegisterRuntimeComponent(flecs::world& world)
    {
        world.component<T>();
    }

    void UpdateWorldTransformRecursive(flecs::entity entityHandle, const Wayfinder::Matrix4& parentMatrix)
    {
        if (!entityHandle.has<Wayfinder::TransformComponent>())
        {
            entityHandle.children([&](flecs::entity child)
            {
                UpdateWorldTransformRecursive(child, parentMatrix);
            });
            return;
        }

        const Wayfinder::TransformComponent& localTransform = entityHandle.get<Wayfinder::TransformComponent>();
        const Wayfinder::Matrix4 localMatrix = localTransform.GetLocalMatrix();
        const Wayfinder::Matrix4 worldMatrix = Wayfinder::Math3D::Multiply(localMatrix, parentMatrix);

        Wayfinder::WorldTransformComponent cachedWorldTransform;
        cachedWorldTransform.LocalToWorld = worldMatrix;
        cachedWorldTransform.Position = Wayfinder::Math3D::TransformPoint(worldMatrix, {0.0f, 0.0f, 0.0f});
        cachedWorldTransform.Scale = Wayfinder::Math3D::ExtractScale(worldMatrix);

        entityHandle.set<Wayfinder::WorldTransformComponent>(cachedWorldTransform);
        entityHandle.children([&](flecs::entity child)
        {
            UpdateWorldTransformRecursive(child, worldMatrix);
        });
    }

    void RegisterTransformModule(flecs::world& world)
    {
        RegisterRuntimeComponent<Wayfinder::WorldTransformComponent>(world);

        world.system<>("UpdateWorldTransforms")
            .kind(flecs::OnUpdate)
            .run([&world](flecs::iter&)
            {
                world.children([&](flecs::entity child)
                {
                    UpdateWorldTransformRecursive(child, Wayfinder::Math3D::Identity());
                });
            });
    }

    void RegisterCameraModule(flecs::world& world)
    {
        RegisterRuntimeComponent<Wayfinder::ActiveCameraStateComponent>(world);

        world.system<>("ExtractActiveCamera")
            .kind(flecs::OnUpdate)
            .run([&world](flecs::iter&)
            {
                Wayfinder::ActiveCameraStateComponent activeCamera;

                world.each([&](flecs::entity entityHandle, const Wayfinder::TransformComponent& transform, const Wayfinder::CameraComponent& camera)
                {
                    if (activeCamera.IsValid || !camera.Primary)
                    {
                        return;
                    }

                    activeCamera.IsValid = true;
                    activeCamera.FieldOfView = camera.FieldOfView;
                    activeCamera.Projection = camera.Projection;

                    if (entityHandle.has<Wayfinder::WorldTransformComponent>())
                    {
                        const auto& worldTransform = entityHandle.get<Wayfinder::WorldTransformComponent>();
                        activeCamera.Position = worldTransform.Position;
                        activeCamera.Target = Wayfinder::Math3D::TransformPoint(worldTransform.LocalToWorld, camera.Target);
                        activeCamera.Up = Wayfinder::Math3D::Normalize(Wayfinder::Math3D::TransformDirection(worldTransform.LocalToWorld, camera.Up));
                    }
                    else
                    {
                        activeCamera.Position = transform.Position;
                        activeCamera.Target = camera.Target;
                        activeCamera.Up = camera.Up;
                    }
                });

                world.set<Wayfinder::ActiveCameraStateComponent>(activeCamera);
            });
    }

    struct ModuleEntry
    {
        const char* Key;
        void (*RegisterFn)(flecs::world& world);
    };

    constexpr std::array<ModuleEntry, 2> kModuleEntries = {{
        {"transform", &RegisterTransformModule},
        {"camera", &RegisterCameraModule},
    }};
}

namespace Wayfinder
{
    const SceneModuleRegistry& SceneModuleRegistry::Get()
    {
        static const SceneModuleRegistry registry;
        return registry;
    }

    void SceneModuleRegistry::RegisterModules(flecs::world& world) const
    {
        for (const ModuleEntry& entry : kModuleEntries)
        {
            entry.RegisterFn(world);
        }
    }
} // namespace Wayfinder