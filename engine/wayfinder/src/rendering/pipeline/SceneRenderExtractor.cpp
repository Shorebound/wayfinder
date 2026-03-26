#include "SceneRenderExtractor.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/SortKey.h"
#include "rendering/materials/Material.h"
#include "rendering/materials/PostProcessVolume.h"

#include "assets/AssetService.h"
#include "core/Log.h"
#include "maths/Bounds.h"
#include "maths/Maths.h"
#include "scene/Components.h"
#include "scene/Scene.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Wayfinder
{
    namespace
    {
        constexpr uint64_t K_BUILT_IN_BOX_MESH_KEY = 1;
        constexpr uint64_t K_BUILT_IN_SURFACE_MATERIAL_KEY = 1;

        uint64_t MakeStableKey(const Wayfinder::AssetId& assetId, uint32_t submeshIndex = 0)
        {
            const std::array<std::uint8_t, 16>& bytes = assetId.Value.GetBytes();
            uint64_t result = 0;
            auto iterator = bytes.begin();
            for (size_t index = 0; index < 8; ++index, ++iterator)
            {
                result = (result << 8) | static_cast<uint64_t>(*iterator);
            }

            /// Mix submesh index into the key to guarantee distinct entries per submesh.
            if (submeshIndex > 0)
            {
                result ^= static_cast<uint64_t>(submeshIndex) * 2654435761ull;
            }

            return result;
        }

        Wayfinder::SortLayer MapLayer(const Wayfinder::RenderLayerId& layer)
        {
            if (layer == Wayfinder::RenderLayers::Overlay)
            {
                return Wayfinder::SortLayer::Overlay;
            }
            return Wayfinder::SortLayer::Opaque;
        }

        uint16_t MaterialIdBits(const Wayfinder::RenderMaterialBinding& material)
        {
            if (material.Ref.AssetId)
            {
                const std::array<std::uint8_t, 16>& bytes = material.Ref.AssetId->Value.GetBytes();
                uint16_t hash = 0;
                for (auto iterator = bytes.begin(); iterator != bytes.end(); std::advance(iterator, 2))
                {
                    const auto lowByte = static_cast<uint16_t>(*iterator);
                    const auto highByte = static_cast<uint16_t>(*std::next(iterator));
                    hash ^= lowByte | static_cast<uint16_t>(highByte << 8);
                }

                return hash;
            }

            uint16_t hash = 0;
            for (const char character : material.ShaderName)
            {
                hash = static_cast<uint16_t>((hash * 131u) ^ static_cast<uint8_t>(character));
            }

            return hash;
        }

        Wayfinder::BlendState MapBlendMode(Wayfinder::MaterialBlendMode mode)
        {
            switch (mode)
            {
            case Wayfinder::MaterialBlendMode::AlphaBlend:
                return Wayfinder::BlendPresets::AlphaBlend();
            case Wayfinder::MaterialBlendMode::Additive:
                return Wayfinder::BlendPresets::Additive();
            case Wayfinder::MaterialBlendMode::Premultiplied:
                return Wayfinder::BlendPresets::Premultiplied();
            case Wayfinder::MaterialBlendMode::Multiplicative:
                return Wayfinder::BlendPresets::Multiplicative();
            default:
                return Wayfinder::BlendPresets::Opaque();
            }
        }

        struct ResolvedMaterialState
        {
            std::string ShaderName;
            Wayfinder::BlendState Blend;
        };

        ResolvedMaterialState ResolveMaterialState(const Wayfinder::RenderMaterialBinding& material, const Wayfinder::Scene& scene)
        {
            ResolvedMaterialState state;
            state.ShaderName = material.ShaderName;
            state.Blend = Wayfinder::BlendPresets::Opaque();

            if (material.Ref.Origin != Wayfinder::RenderResourceOrigin::Asset || !material.Ref.AssetId)
            {
                return state;
            }

            const std::shared_ptr<Wayfinder::AssetService>& assetService = scene.GetAssetService();
            if (!assetService)
            {
                return state;
            }

            std::string error;
            const Wayfinder::MaterialAsset* materialAsset = assetService->LoadMaterialAsset(*material.Ref.AssetId, error);
            if (!materialAsset)
            {
                return state;
            }

            state.ShaderName = materialAsset->ShaderName;
            state.Blend = MapBlendMode(materialAsset->BlendMode);
            return state;
        }

        uint8_t BlendGroupBits(const Wayfinder::BlendState& blendState)
        {
            if (!blendState.Enabled)
            {
                return 0;
            }

            if (blendState == Wayfinder::BlendPresets::AlphaBlend())
            {
                return 1;
            }

            if (blendState == Wayfinder::BlendPresets::Additive())
            {
                return 2;
            }

            if (blendState == Wayfinder::BlendPresets::Premultiplied())
            {
                return 3;
            }

            if (blendState == Wayfinder::BlendPresets::Multiplicative())
            {
                return 4;
            }

            uint32_t hash = 17;
            hash = hash * 31u + static_cast<uint8_t>(blendState.SrcColourFactor);
            hash = hash * 31u + static_cast<uint8_t>(blendState.DstColourFactor);
            hash = hash * 31u + static_cast<uint8_t>(blendState.ColourOp);
            hash = hash * 31u + static_cast<uint8_t>(blendState.SrcAlphaFactor);
            hash = hash * 31u + static_cast<uint8_t>(blendState.DstAlphaFactor);
            hash = hash * 31u + static_cast<uint8_t>(blendState.AlphaOp);
            hash = hash * 31u + blendState.ColourWriteMask;

            return static_cast<uint8_t>(5u + (hash % 59u));
        }

    } // namespace
}

namespace Wayfinder
{
    RenderFrame SceneRenderExtractor::Extract(const Scene& scene) const
    {
        RenderFrame frame;
        frame.SceneName = scene.GetName();
        frame.AssetRoot = scene.GetAssetRoot();

        if (scene.GetWorld().has<ActiveCameraStateComponent>())
        {
            const auto& activeCamera = scene.GetWorld().get<ActiveCameraStateComponent>();
            if (activeCamera.IsValid)
            {
                RenderView view;
                view.CameraState.Position = activeCamera.Position;
                view.CameraState.Target = activeCamera.Target;
                view.CameraState.Up = activeCamera.Up;
                view.CameraState.FOV = activeCamera.FieldOfView;
                view.CameraState.ProjectionType = static_cast<int>(activeCamera.Projection);
                const size_t viewIndex = frame.AddView(view);
                frame.AddScenePass(RenderPassIds::MainScene, viewIndex, RenderLayers::Main);
                frame.AddScenePass(RenderPassIds::OverlayScene, viewIndex, RenderLayers::Overlay);
                RenderPass& debugPass = frame.AddDebugPass(RenderPassIds::Debug, viewIndex);
                if (debugPass.DebugDraw)
                {
                    debugPass.DebugDraw->ShowWorldGrid = true;
                }
            }
        }

        // Compute view matrix for sort-key depth calculation
        auto cameraView = Matrix4(1.0f);
        if (scene.GetWorld().has<ActiveCameraStateComponent>())
        {
            const auto& activeCamera = scene.GetWorld().get<ActiveCameraStateComponent>();
            if (activeCamera.IsValid)
            {
                cameraView = Maths::LookAt(activeCamera.Position, activeCamera.Target, activeCamera.Up);
            }
        }

        scene.GetWorld().each([&frame, &cameraView, &scene](flecs::entity entityHandle)
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

            // Compute camera-space Z for depth sorting (shared by all submesh submissions)
            const Float3 worldPosition = Maths::TransformPoint(localToWorld, {0.0f, 0.0f, 0.0f});
            const Float3 cameraSpacePosition = Maths::TransformPoint(cameraView, worldPosition);
            const float cameraSpaceZ = cameraSpacePosition.z; // NOLINT(cppcoreguidelines-pro-type-union-access)

            // Resolve entity-level material (fallback for all submeshes)
            RenderMaterialBinding entityMaterial{};
            entityMaterial.Ref.Origin = RenderResourceOrigin::BuiltIn;
            entityMaterial.Ref.StableKey = K_BUILT_IN_SURFACE_MATERIAL_KEY;
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

            // Resolve render-state overrides
            RenderStateOverrides stateOverrides{};
            if (entityHandle.has<RenderOverrideComponent>())
            {
                const auto& renderOverride = entityHandle.get<RenderOverrideComponent>();
                if (renderOverride.Wireframe.has_value())
                {
                    stateOverrides.FillMode = *renderOverride.Wireframe ? RenderFillMode::SolidAndWireframe : RenderFillMode::Solid;
                }
            }

            /// Emit a single submission with the given mesh ref, material, and world-space bounds.
            auto emitSubmission = [&](const RenderMeshRef& meshRef, const RenderMaterialBinding& material, const AxisAlignedBounds& worldBounds)
            {
                RenderMeshSubmission submission;
                submission.Mesh = meshRef;
                submission.Geometry.Type = RenderGeometryType::Box;
                submission.Geometry.Dimensions = mesh.Dimensions;
                submission.Material = material;
                submission.Material.StateOverrides = stateOverrides;
                const auto materialState = ResolveMaterialState(submission.Material, scene);
                submission.Material.ShaderName = materialState.ShaderName;
                submission.Visible = renderable.Visible;
                submission.Layer = renderable.Layer;
                submission.SortPriority = renderable.SortPriority;
                submission.LocalToWorld = localToWorld;
                submission.WorldBounds = worldBounds;
                submission.WorldSphere = ComputeBoundingSphere(worldBounds);
                const SortLayer sortLayer = materialState.Blend.Enabled ? SortLayer::Transparent : MapLayer(submission.Layer);
                submission.SortKey = SortKeyBuilder::Build(sortLayer, BlendGroupBits(materialState.Blend), MaterialIdBits(submission.Material), cameraSpaceZ, submission.SortPriority);

                RenderPass* owningPass = frame.FindScenePassForSubmission(submission, 0);
                if (!owningPass)
                {
                    WAYFINDER_WARNING(LogRenderer, "SceneRenderExtractor skipped mesh submission because no scene pass matched layer '{0}' in frame '{1}'.", submission.Layer, frame.SceneName);
                    return;
                }

                owningPass->Meshes.push_back(std::move(submission));
            };

            if (mesh.MeshAssetId)
            {
                // Asset mesh — load metadata once, emit one submission per submesh
                const MeshAsset* meshAsset = nullptr;
                const auto& assetService = scene.GetAssetService();
                if (assetService)
                {
                    std::string error;
                    meshAsset = assetService->LoadAsset<MeshAsset>(*mesh.MeshAssetId, error);
                }

                const uint32_t submeshCount = meshAsset ? static_cast<uint32_t>(meshAsset->Submeshes.size()) : 1u;

                for (uint32_t submeshIdx = 0; submeshIdx < submeshCount; ++submeshIdx)
                {
                    RenderMeshRef meshRef;
                    meshRef.Origin = RenderResourceOrigin::Asset;
                    meshRef.AssetId = mesh.MeshAssetId;
                    meshRef.StableKey = MakeStableKey(*mesh.MeshAssetId, submeshIdx);
                    meshRef.SubmeshIndex = submeshIdx;

                    // Resolve per-submesh material: slot binding → entity material → built-in
                    RenderMaterialBinding submeshMaterial = entityMaterial;

                    uint32_t materialSlot = submeshIdx;
                    AxisAlignedBounds localBounds{};
                    if (meshAsset && submeshIdx < meshAsset->Submeshes.size())
                    {
                        materialSlot = meshAsset->Submeshes.at(submeshIdx).MaterialSlot;
                        localBounds = meshAsset->Submeshes.at(submeshIdx).Bounds;
                    }

                    if (const auto slotIt = mesh.MaterialSlotBindings.find(materialSlot); slotIt != mesh.MaterialSlotBindings.end())
                    {
                        submeshMaterial.Ref.Origin = RenderResourceOrigin::Asset;
                        submeshMaterial.Ref.AssetId = slotIt->second;
                        submeshMaterial.Ref.StableKey = MakeStableKey(slotIt->second);
                        submeshMaterial.HasOverrides = false;
                    }

                    const AxisAlignedBounds worldBounds = TransformBounds(localBounds, localToWorld);
                    emitSubmission(meshRef, submeshMaterial, worldBounds);
                }
            }
            else
            {
                // Built-in primitive — single submission with bounds derived from dimensions
                RenderMeshRef meshRef;
                meshRef.Origin = RenderResourceOrigin::BuiltIn;
                meshRef.StableKey = K_BUILT_IN_BOX_MESH_KEY;

                const Float3 halfDim = mesh.Dimensions * 0.5f;
                const AxisAlignedBounds localBounds{.Min = -halfDim, .Max = halfDim};
                const AxisAlignedBounds worldBounds = TransformBounds(localBounds, localToWorld);

                emitSubmission(meshRef, entityMaterial, worldBounds);
            }
        });

        scene.GetWorld().each([&frame](flecs::entity entityHandle)
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
            frame.Lights.push_back(submission);

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

                if (RenderPass* pass = frame.FindPass(RenderPassIds::Debug))
                {
                    pass->DebugDraw->Boxes.push_back(debugBox);
                }

                if (light.Type == LightType::Directional)
                {
                    const Float3 lineEnd = Maths::Add(position, Maths::Scale(direction, 1.5f));
                    RenderDebugLine debugLine;
                    debugLine.Start = position;
                    debugLine.End = lineEnd;
                    debugLine.Tint = light.Tint;

                    if (RenderPass* pass = frame.FindPass(RenderPassIds::Debug))
                    {
                        pass->DebugDraw->Lines.push_back(debugLine);
                    }
                }
            }
        });

        // Extract post-process volumes (global volumes may have no transform)
        std::vector<PostProcessVolumeInstance> volumeInstances;
        scene.GetWorld().each([&volumeInstances](flecs::entity entityHandle)
        {
            if (!entityHandle.has<PostProcessVolumeComponent>())
            {
                return;
            }

            const auto& volume = entityHandle.get<PostProcessVolumeComponent>();

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

        // Blend post-process volumes per view using each view's camera position
        if (!volumeInstances.empty())
        {
            for (auto& view : frame.Views)
            {
                view.PostProcess = BlendPostProcessVolumes(view.CameraState.Position, volumeInstances);
            }
        }

        return frame;
    }
} // namespace Wayfinder
