#include "SceneRenderExtractor.h"

#include "rendering/graph/Canvas.h"
#include "rendering/graph/RenderFrame.h"
#include "scene/Components.h"
#include "volumes/BlendableEffect.h"
#include "volumes/BlendableEffectRegistry.h"

#include "core/Log.h"
#include "maths/Bounds.h"
#include "maths/Maths.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Wayfinder
{
    namespace
    {
        constexpr uint64_t BUILT_IN_BOX_MESH_KEY = 1;
        constexpr uint64_t BUILT_IN_SURFACE_MATERIAL_KEY = 1;

        uint64_t MakeStableKey(const AssetId& assetId, uint32_t submeshIndex = 0)
        {
            const std::array<std::uint8_t, 16>& bytes = assetId.Value.GetBytes();
            uint64_t result = 0;
            for (size_t index = 0; index < 8; ++index)
            {
                result = (result << 8) | static_cast<uint64_t>(bytes[index]);
            }

            if (submeshIndex > 0)
            {
                result ^= static_cast<uint64_t>(submeshIndex) * 2654435761ull;
            }

            return result;
        }

    } // namespace

    SceneRenderExtractor::SceneRenderExtractor(flecs::world& world)
        : m_world(world)
    {
    }

    namespace
    {
        std::optional<size_t> ExtractViews(const flecs::world& world, SceneCanvas& canvas)
        {
            if (!world.has<ActiveCameraStateComponent>())
            {
                return std::nullopt;
            }

            const auto& activeCamera = world.get<ActiveCameraStateComponent>();
            if (!activeCamera.IsValid)
            {
                return std::nullopt;
            }

            RenderView view;
            view.CameraState.Position = activeCamera.Position;
            view.CameraState.Target = activeCamera.Target;
            view.CameraState.Up = activeCamera.Up;
            view.CameraState.FOV = activeCamera.FieldOfView;
            view.CameraState.ProjectionType = static_cast<int>(activeCamera.Projection);
            const size_t viewIndex = canvas.AddView(view);

            return viewIndex;
        }

        void ExtractMeshSubmissions(const flecs::world& world, SceneCanvas& canvas, std::optional<size_t> primaryViewIndex)
        {
            if (!primaryViewIndex.has_value())
            {
                return;
            }

            world.each([&canvas, primaryViewIndex](flecs::entity entityHandle)
            {
                if (!entityHandle.has<TransformComponent>() || !entityHandle.has<MeshComponent>() || !entityHandle.has<RenderableComponent>())
                {
                    return;
                }

                const auto& transform = entityHandle.get<TransformComponent>();
                const auto& mesh = entityHandle.get<MeshComponent>();
                const auto& renderable = entityHandle.get<RenderableComponent>();

                Matrix4 localToWorld = transform.GetLocalMatrix();

                if (entityHandle.has<WorldTransformComponent>())
                {
                    const auto& worldTransform = entityHandle.get<WorldTransformComponent>();
                    localToWorld = worldTransform.LocalToWorld;
                }

                RenderMaterialBinding entityMaterial{};
                entityMaterial.Ref.Origin = RenderResourceOrigin::BuiltIn;
                entityMaterial.Ref.StableKey = BUILT_IN_SURFACE_MATERIAL_KEY;
                entityMaterial.Domain = RenderMaterialDomain::Surface;
                entityMaterial.Parameters.SetColour("base_colour", LinearColour::White());

                if (entityHandle.has<MaterialComponent>())
                {
                    const auto& material = entityHandle.get<MaterialComponent>();
                    if (material.MaterialAssetId)
                    {
                        entityMaterial.Ref.Origin = RenderResourceOrigin::Asset;
                        entityMaterial.Ref.AssetId = material.MaterialAssetId;
                        entityMaterial.Ref.StableKey = MakeStableKey(*material.MaterialAssetId);
                    }

                    if (material.HasBaseColourOverride || !material.MaterialAssetId)
                    {
                        entityMaterial.HasOverrides = true;
                        entityMaterial.Overrides.SetColour("base_colour", LinearColour::FromColour(material.BaseColour));
                    }
                }

                RenderStateOverrides stateOverrides{};
                if (entityHandle.has<RenderOverrideComponent>())
                {
                    const auto& renderOverride = entityHandle.get<RenderOverrideComponent>();
                    if (renderOverride.Wireframe.has_value())
                    {
                        stateOverrides.FillMode = *renderOverride.Wireframe ? RenderFillMode::SolidAndWireframe : RenderFillMode::Solid;
                    }
                }

                auto emitSubmission = [&](const RenderMeshRef& meshRef, const RenderMaterialBinding& materialBinding)
                {
                    RenderMeshSubmission submission;
                    submission.Mesh = meshRef;
                    submission.Geometry.Type = RenderGeometryType::Box;
                    submission.Geometry.Dimensions = mesh.Dimensions;
                    submission.Material = materialBinding;
                    submission.Material.StateOverrides = stateOverrides;
                    submission.Visible = renderable.Visible;
                    submission.Group = renderable.Group;
                    submission.SortPriority = renderable.SortPriority;
                    submission.LocalToWorld = localToWorld;

                    const Float3 halfDim = mesh.Dimensions * 0.5f;
                    const AxisAlignedBounds localBounds{.Min = -halfDim, .Max = halfDim};
                    submission.WorldBounds = TransformBounds(localBounds, localToWorld);
                    submission.WorldSphere = ComputeBoundingSphere(submission.WorldBounds);

                    // Sort key computation deferred to the renderer (Phase 6).
                    // SceneCanvas consumers (render features) apply sorting downstream.
                    submission.ViewIndex = primaryViewIndex;

                    canvas.SubmitMesh(std::move(submission));
                };

                if (mesh.MeshAssetId)
                {
                    // For asset meshes without Scene access, emit a single submission.
                    // Full multi-submesh resolution requires AssetService (Phase 6).
                    RenderMeshRef meshRef;
                    meshRef.Origin = RenderResourceOrigin::Asset;
                    meshRef.AssetId = mesh.MeshAssetId;
                    meshRef.StableKey = MakeStableKey(*mesh.MeshAssetId);

                    emitSubmission(meshRef, entityMaterial);
                }
                else
                {
                    RenderMeshRef meshRef;
                    meshRef.Origin = RenderResourceOrigin::BuiltIn;
                    meshRef.StableKey = BUILT_IN_BOX_MESH_KEY;

                    emitSubmission(meshRef, entityMaterial);
                }
            });
        }

        void ExtractLights(const flecs::world& world, SceneCanvas& canvas)
        {
            world.each([&canvas](flecs::entity entityHandle)
            {
                if (!entityHandle.has<TransformComponent>() || !entityHandle.has<LightComponent>())
                {
                    return;
                }

                const auto& transform = entityHandle.get<TransformComponent>();
                const auto& light = entityHandle.get<LightComponent>();

                Matrix4 localToWorld = transform.GetLocalMatrix();
                Float3 position = transform.Local.Position;
                if (entityHandle.has<WorldTransformComponent>())
                {
                    const auto& worldTransform = entityHandle.get<WorldTransformComponent>();
                    localToWorld = worldTransform.LocalToWorld;
                    position = worldTransform.Position;
                }

                const Float3 direction = Maths::Normalize(Maths::TransformDirection(localToWorld, {0.0f, 0.0f, -1.0f}));

                RenderLightSubmission submission;
                submission.Type = light.Type == LightType::Directional ? RenderLightType::Directional : RenderLightType::Point;
                submission.Position = position;
                submission.Direction = direction;
                submission.Tint = light.Tint;
                submission.Intensity = light.Intensity;
                submission.Range = light.Range;
                submission.DebugDraw = light.DebugDraw;
                canvas.SubmitLight(std::move(submission));

                // Debug draw for lights writes to SceneCanvas::DebugDraw
                if (light.DebugDraw)
                {
                    const float debugSize = light.Type == LightType::Directional ? 0.6f : 0.3f;
                    const Matrix4 debugTransform = Maths::ComposeTransform({
                        .Position = position,
                        .RotationDegrees = {0.0f, 0.0f, 0.0f},
                        .Scale = {debugSize, debugSize, debugSize},
                    });

                    RenderDebugBox debugBox;
                    debugBox.LocalToWorld = debugTransform;
                    debugBox.Dimensions = {1.0f, 1.0f, 1.0f};
                    debugBox.Material.Ref.Origin = RenderResourceOrigin::BuiltIn;
                    debugBox.Material.Ref.StableKey = 100ull;
                    debugBox.Material.Domain = RenderMaterialDomain::Debug;
                    debugBox.Material.Parameters.SetColour("base_colour", LinearColour::FromColour(light.Tint));

                    canvas.DebugDraw.Boxes.push_back(debugBox);

                    if (light.Type == LightType::Directional)
                    {
                        const Float3 lineEnd = Maths::Add(position, Maths::Scale(direction, 1.5f));
                        RenderDebugLine debugLine;
                        debugLine.Start = position;
                        debugLine.End = lineEnd;
                        debugLine.Tint = light.Tint;

                        canvas.DebugDraw.Lines.push_back(debugLine);
                    }
                }
            });
        }

        void ExtractPostProcessVolumes(const flecs::world& world, SceneCanvas& canvas, const BlendableEffectRegistry* registry)
        {
            std::vector<VolumeInstance> volumeInstances;
            world.each([&volumeInstances](flecs::entity entityHandle)
            {
                if (!entityHandle.has<BlendableEffectVolumeComponent>())
                {
                    return;
                }

                const auto& volume = entityHandle.get<BlendableEffectVolumeComponent>();

                Float3 position{0.0f, 0.0f, 0.0f};
                Float3 scale{1.0f, 1.0f, 1.0f};
                auto localToWorld = Matrix4(1.0f);

                if (entityHandle.has<WorldTransformComponent>())
                {
                    const auto& worldTransform = entityHandle.get<WorldTransformComponent>();
                    position = worldTransform.Position;
                    scale = worldTransform.Scale;
                    localToWorld = worldTransform.LocalToWorld;
                }
                else if (entityHandle.has<TransformComponent>())
                {
                    const auto& transform = entityHandle.get<TransformComponent>();
                    position = transform.Local.Position;
                    scale = transform.Local.Scale;
                    localToWorld = transform.GetLocalMatrix();
                }

                volumeInstances.push_back({.Volume = &volume, .WorldPosition = position, .WorldScale = scale, .LocalToWorld = localToWorld});
            });

            if (volumeInstances.empty())
            {
                return;
            }

            if (registry != nullptr)
            {
                // Use camera position from the first view if available
                Float3 cameraPosition{0.0f, 0.0f, 0.0f};
                if (!canvas.Views.empty())
                {
                    cameraPosition = canvas.Views[0].CameraState.Position;
                }

                canvas.PostProcess = BlendVolumeEffects(cameraPosition, volumeInstances, *registry);
            }
        }

    } // namespace

    void SceneRenderExtractor::Extract(SceneCanvas& canvas) const
    {
        Extract(canvas, nullptr);
    }

    void SceneRenderExtractor::Extract(SceneCanvas& canvas, const BlendableEffectRegistry* registry) const
    {
        const auto primaryViewIndex = ExtractViews(m_world, canvas);
        ExtractMeshSubmissions(m_world, canvas, primaryViewIndex);
        ExtractLights(m_world, canvas);
        ExtractPostProcessVolumes(m_world, canvas, registry);
    }

} // namespace Wayfinder
