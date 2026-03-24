#include "SceneRenderExtractor.h"
#include "rendering/graph/SortKey.h"
#include "rendering/materials/PostProcessVolume.h"

#include "core/Log.h"
#include "maths/Maths.h"
#include "scene/Components.h"
#include "scene/Scene.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

namespace Wayfinder
{
    namespace
    {
        constexpr uint64_t K_BUILT_IN_BOX_MESH_KEY = 1;
        constexpr uint64_t K_BUILT_IN_SURFACE_MATERIAL_KEY = 1;

        uint64_t MakeStableKey(const Wayfinder::AssetId& assetId)
        {
            const std::array<std::uint8_t, 16>& bytes = assetId.Value.GetBytes();
            uint64_t result = 0;
            auto iterator = bytes.begin();
            for (size_t index = 0; index < 8; ++index, ++iterator)
            {
                result = (result << 8) | static_cast<uint64_t>(*iterator);
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

        uint16_t MaterialIdBits(const std::optional<Wayfinder::AssetId>& assetId)
        {
            if (!assetId)
            {
                return 0;
            }

            const std::array<std::uint8_t, 16>& bytes = assetId->Value.GetBytes();
            uint16_t hash = 0;
            for (auto iterator = bytes.begin(); iterator != bytes.end(); std::advance(iterator, 2))
            {
                const auto lowByte = static_cast<uint16_t>(*iterator);
                const auto highByte = static_cast<uint16_t>(*std::next(iterator));
                hash ^= lowByte | static_cast<uint16_t>(highByte << 8);
            }

            return hash;
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

        scene.GetWorld().each([&frame, &cameraView](flecs::entity entityHandle)
        {
            if (!entityHandle.has<TransformComponent>() || !entityHandle.has<MeshComponent>() || !entityHandle.has<RenderableComponent>())
            {
                return;
            }

            const auto& transform = entityHandle.get<TransformComponent>();
            const auto& mesh = entityHandle.get<MeshComponent>();
            const auto& renderable = entityHandle.get<RenderableComponent>();

            RenderMeshSubmission submission;
            if (mesh.MeshAssetId)
            {
                submission.Mesh.Origin = RenderResourceOrigin::Asset;
                submission.Mesh.AssetId = mesh.MeshAssetId;
                submission.Mesh.StableKey = MakeStableKey(*mesh.MeshAssetId);
            }
            else
            {
                submission.Mesh.Origin = RenderResourceOrigin::BuiltIn;
                submission.Mesh.StableKey = K_BUILT_IN_BOX_MESH_KEY;
            }
            submission.Geometry.Type = RenderGeometryType::Box;
            submission.Geometry.Dimensions = mesh.Dimensions;
            submission.Material.Ref.Origin = RenderResourceOrigin::BuiltIn;
            submission.Material.Ref.StableKey = K_BUILT_IN_SURFACE_MATERIAL_KEY;
            submission.Material.Domain = RenderMaterialDomain::Surface;
            submission.Material.Parameters.SetColour("base_colour", LinearColour::White());

            Matrix4 localToWorld = transform.GetLocalMatrix();

            if (entityHandle.has<WorldTransformComponent>())
            {
                const auto& worldTransform = entityHandle.get<WorldTransformComponent>();
                localToWorld = worldTransform.LocalToWorld;
            }

            if (entityHandle.has<MaterialComponent>())
            {
                const auto& material = entityHandle.get<MaterialComponent>();
                if (material.MaterialAssetId)
                {
                    submission.Material.Ref.Origin = RenderResourceOrigin::Asset;
                    submission.Material.Ref.AssetId = material.MaterialAssetId;
                    submission.Material.Ref.StableKey = MakeStableKey(*material.MaterialAssetId);
                }

                if (material.HasBaseColourOverride || !material.MaterialAssetId)
                {
                    submission.Material.HasOverrides = true;
                    submission.Material.Overrides.SetColour("base_colour", LinearColour::FromColour(material.BaseColour));
                }
            }

            if (entityHandle.has<RenderOverrideComponent>())
            {
                const auto& renderOverride = entityHandle.get<RenderOverrideComponent>();
                if (renderOverride.Wireframe.has_value())
                {
                    submission.Material.StateOverrides.FillMode = *renderOverride.Wireframe ? RenderFillMode::SolidAndWireframe : RenderFillMode::Solid;
                }
            }

            submission.Visible = renderable.Visible;
            submission.Layer = renderable.Layer;
            submission.SortPriority = renderable.SortPriority;

            submission.LocalToWorld = localToWorld;

            // Compute camera-space Z for depth sorting
            const Float3 worldPosition = Maths::TransformPoint(localToWorld, {0.0f, 0.0f, 0.0f});
            const Float3 cameraSpacePosition = Maths::TransformPoint(cameraView, worldPosition);
            const float cameraSpaceZ = cameraSpacePosition.z; // NOLINT(cppcoreguidelines-pro-type-union-access)

            submission.SortKey = SortKeyBuilder::Build(MapLayer(submission.Layer), MaterialIdBits(submission.Material.Ref.AssetId), cameraSpaceZ, static_cast<uint16_t>(submission.SortPriority));

            RenderPass* owningPass = frame.FindScenePassForSubmission(submission, 0);
            if (!owningPass)
            {
                WAYFINDER_WARNING(LogRenderer, "SceneRenderExtractor skipped mesh submission because no scene pass matched layer '{0}' in frame '{1}'.", submission.Layer, frame.SceneName);
                return;
            }

            owningPass->Meshes.push_back(std::move(submission));
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
