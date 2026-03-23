#include "TransformPlugin.h"

#include "maths/Maths.h"
#include "plugins/PluginRegistry.h"
#include "scene/Components.h"

#include "ecs/Flecs.h"

namespace Wayfinder
{
    namespace
    {
        void RegisterWorldTransformType(flecs::world& world)
        {
            world.component<WorldTransformComponent>();
        }
    } // namespace

    void TransformPlugin::Build(PluginRegistry& registry)
    {
        registry.RegisterComponent({.Key = "world_transform", .RegisterFn = RegisterWorldTransformType});
        registry.RegisterSystem("UpdateWorldTransforms", [](flecs::world& world)
        {
            // NOLINTNEXTLINE(clang-analyzer-core.StackAddressEscape)
            world.system<>("UpdateWorldTransforms")
                .kind(flecs::PreUpdate)
                .run([&world](flecs::iter&)
            {
                world.children([&](flecs::entity child)
                {
                    struct TransformPropagation
                    {
                        static void UpdateRecursive(flecs::entity entityHandle, const Matrix4& parentMatrix)
                        {
                            if (!entityHandle.has<TransformComponent>())
                            {
                                entityHandle.children([&](flecs::entity childEntity)
                                {
                                    UpdateRecursive(childEntity, parentMatrix);
                                });
                                return;
                            }

                            const auto& localTransform = entityHandle.get<TransformComponent>();
                            const Matrix4 localMatrix = localTransform.GetLocalMatrix();
                            const Matrix4 worldMatrix = Maths::Multiply(localMatrix, parentMatrix);

                            WorldTransformComponent cachedWorldTransform;
                            cachedWorldTransform.LocalToWorld = worldMatrix;
                            cachedWorldTransform.Position = Maths::TransformPoint(worldMatrix, {0.0f, 0.0f, 0.0f});
                            cachedWorldTransform.Scale = Maths::ExtractScale(worldMatrix);

                            entityHandle.set<WorldTransformComponent>(cachedWorldTransform);
                            entityHandle.children([&](flecs::entity childEntity)
                            {
                                UpdateRecursive(childEntity, worldMatrix);
                            });
                        }
                    };
                    TransformPropagation::UpdateRecursive(child, Maths::Identity());
                });
            });
        });
    }

} // namespace Wayfinder
