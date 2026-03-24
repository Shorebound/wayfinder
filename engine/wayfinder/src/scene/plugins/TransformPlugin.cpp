#include "TransformPlugin.h"

#include "maths/Maths.h"
#include "plugins/PluginRegistry.h"
#include "scene/Components.h"

#include "ecs/Flecs.h"

#include <vector>

namespace Wayfinder
{
    namespace
    {
        void RegisterWorldTransformType(flecs::world& world)
        {
            world.component<WorldTransformComponent>();
        }
    } // namespace

    void TransformPlugin::Build(Plugins::PluginRegistry& registry)
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
                    std::vector<std::pair<flecs::entity, Matrix4>> stack;
                    stack.emplace_back(child, Maths::Identity());
                    while (!stack.empty())
                    {
                        auto [entityHandle, parentMatrix] = stack.back();
                        stack.pop_back();

                        if (!entityHandle.has<TransformComponent>())
                        {
                            std::vector<flecs::entity> childList;
                            entityHandle.children([&](flecs::entity childEntity)
                            {
                                childList.push_back(childEntity);
                            });
                            for (auto it = childList.rbegin(); it != childList.rend(); ++it)
                            {
                                stack.emplace_back(*it, parentMatrix);
                            }
                            continue;
                        }

                        const auto& localTransform = entityHandle.get<TransformComponent>();
                        const Matrix4 localMatrix = localTransform.GetLocalMatrix();
                        const Matrix4 worldMatrix = Maths::Multiply(localMatrix, parentMatrix);

                        WorldTransformComponent cachedWorldTransform;
                        cachedWorldTransform.LocalToWorld = worldMatrix;
                        cachedWorldTransform.Position = Maths::TransformPoint(worldMatrix, {0.0f, 0.0f, 0.0f});
                        cachedWorldTransform.Scale = Maths::ExtractScale(worldMatrix);

                        entityHandle.set<WorldTransformComponent>(cachedWorldTransform);

                        std::vector<flecs::entity> childList;
                        entityHandle.children([&](flecs::entity childEntity)
                        {
                            childList.push_back(childEntity);
                        });
                        for (auto it = childList.rbegin(); it != childList.rend(); ++it)
                        {
                            stack.emplace_back(*it, worldMatrix);
                        }
                    }
                });
            });
        });
    }

} // namespace Wayfinder
