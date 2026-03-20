#include "SceneRenderExtractor.h"
#include "PostProcessVolume.h"
#include "SortKey.h"

#include "../core/Log.h"
#include "../scene/Components.h"
#include "../scene/Scene.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
    constexpr uint64_t kBuiltInBoxMeshKey = 1;
    constexpr uint64_t kBuiltInSurfaceMaterialKey = 1;

    uint64_t MakeStableKey(const Wayfinder::AssetId& assetId)
    {
        const std::array<std::uint8_t, 16>& bytes = assetId.Value.GetBytes();
        uint64_t result = 0;
        for (size_t index = 0; index < 8; ++index)
        {
            result = (result << 8) | static_cast<uint64_t>(bytes[index]);
        }

        return result;
    }

    Wayfinder::SortLayer MapLayer(const Wayfinder::RenderLayerId& layer)
    {
        if (layer == Wayfinder::RenderLayers::Overlay) return Wayfinder::SortLayer::Overlay;
        return Wayfinder::SortLayer::Opaque;
    }

    uint16_t MaterialIdBits(const std::optional<Wayfinder::AssetId>& assetId)
    {
        if (!assetId) return 0;
        // XOR-fold all 16 UUID bytes into 16 bits for better distribution
        const std::array<std::uint8_t, 16>& bytes = assetId->Value.GetBytes();
        uint16_t hash = 0;
        for (size_t i = 0; i < 16; i += 2)
        {
            hash ^= static_cast<uint16_t>(bytes[i]) | (static_cast<uint16_t>(bytes[i + 1]) << 8);
        }
        return hash;
    }
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
            const ActiveCameraStateComponent& activeCamera = scene.GetWorld().get<ActiveCameraStateComponent>();
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
                debugPass.DebugDraw->ShowWorldGrid = true;
            }
        }

        // Compute view matrix for sort-key depth calculation
        Matrix4 cameraView = glm::mat4(1.0f);
        if (scene.GetWorld().has<ActiveCameraStateComponent>())
        {
            const ActiveCameraStateComponent& activeCamera = scene.GetWorld().get<ActiveCameraStateComponent>();
            if (activeCamera.IsValid)
            {
                cameraView = glm::lookAt(activeCamera.Position, activeCamera.Target, activeCamera.Up);
            }
        }

        scene.GetWorld().each([&frame, &cameraView](flecs::entity entityHandle, const TransformComponent& transform, const MeshComponent& mesh, const RenderableComponent& renderable)
        {
            RenderMeshSubmission submission;
            submission.Mesh.Origin = RenderResourceOrigin::BuiltIn;
            submission.Mesh.StableKey = kBuiltInBoxMeshKey;
            submission.Geometry.Type = RenderGeometryType::Box;
            submission.Geometry.Dimensions = mesh.Dimensions;
            submission.Material.Ref.Origin = RenderResourceOrigin::BuiltIn;
            submission.Material.Ref.StableKey = kBuiltInSurfaceMaterialKey;
            submission.Material.Domain = RenderMaterialDomain::Surface;
            submission.Material.Parameters.SetColor("base_color", LinearColor::White());
            submission.Material.StateOverrides.FillMode = RenderFillMode::SolidAndWireframe;

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

                if (material.HasBaseColorOverride || !material.MaterialAssetId)
                {
                    submission.Material.HasOverrides = true;
                    submission.Material.Overrides.SetColor("base_color", LinearColor::FromColor(material.BaseColor));
                }

                if (material.HasWireframeOverride || !material.MaterialAssetId)
                {
                    submission.Material.StateOverrides.FillMode = material.Wireframe
                        ? RenderFillMode::SolidAndWireframe
                        : RenderFillMode::Solid;
                }
            }

            submission.Visible = renderable.Visible;
            submission.Layer = renderable.Layer;
            submission.SortPriority = renderable.SortPriority;

            submission.LocalToWorld = localToWorld;

            // Compute camera-space Z for depth sorting
            const glm::vec4 worldPos = glm::vec4(glm::vec3(localToWorld[3]), 1.0f);
            const float cameraSpaceZ = (cameraView * worldPos).z;

            submission.SortKey = SortKeyBuilder::Build(
                MapLayer(submission.Layer),
                MaterialIdBits(submission.Material.Ref.AssetId),
                cameraSpaceZ,
                static_cast<uint16_t>(submission.SortPriority));

            RenderPass* owningPass = frame.FindScenePassForSubmission(submission, 0);
            if (!owningPass)
            {
                WAYFINDER_WARNING(LogRenderer, "SceneRenderExtractor skipped mesh submission because no scene pass matched layer '{0}' in frame '{1}'.", submission.Layer, frame.SceneName);
                return;
            }

            owningPass->Meshes.push_back(std::move(submission));
        });

        scene.GetWorld().each([&frame](flecs::entity entityHandle, const TransformComponent& transform, const LightComponent& light)
        {
            Matrix4 localToWorld = transform.GetLocalMatrix();
            Float3 position = transform.Position;
            if (entityHandle.has<WorldTransformComponent>())
            {
                const auto& worldTransform = entityHandle.get<WorldTransformComponent>();
                localToWorld = worldTransform.LocalToWorld;
                position = worldTransform.Position;
            }

            const Float3 direction = Math3D::Normalize(Math3D::TransformDirection(localToWorld, {0.0f, 0.0f, -1.0f}));

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
                const Matrix4 debugTransform = Math3D::ComposeTransform(position, {0.0f, 0.0f, 0.0f}, {debugSize, debugSize, debugSize});

                RenderDebugBox debugBox;
                debugBox.LocalToWorld = debugTransform;
                debugBox.Dimensions = {1.0f, 1.0f, 1.0f};
                debugBox.Material.Ref.Origin = RenderResourceOrigin::BuiltIn;
                debugBox.Material.Ref.StableKey = 100ull;
                debugBox.Material.Domain = RenderMaterialDomain::Debug;
                debugBox.Material.Parameters.SetColor("base_color", LinearColor::FromColor(light.Tint));

                if (RenderPass* pass = frame.FindPass(RenderPassIds::Debug))
                {
                    pass->DebugDraw->Boxes.push_back(debugBox);
                }

                if (light.Type == LightType::Directional)
                {
                    const Float3 lineEnd = Math3D::Add(position, Math3D::Scale(direction, 1.5f));
                    RenderDebugLine debugLine;
                    debugLine.Start = position;
                    debugLine.End = lineEnd;
                    debugLine.Color = light.Tint;

                    if (RenderPass* pass = frame.FindPass(RenderPassIds::Debug))
                    {
                        pass->DebugDraw->Lines.push_back(debugLine);
                    }
                }
            }
        });

        // Extract post-process volumes (global volumes may have no transform)
        std::vector<PostProcessVolumeInstance> volumeInstances;
        scene.GetWorld().each([&volumeInstances](flecs::entity entityHandle, const PostProcessVolumeComponent& volume)
        {
            Float3 position{0.0f, 0.0f, 0.0f};
            Float3 scale{1.0f, 1.0f, 1.0f};
            Matrix4 localToWorld = Matrix4(1.0f);

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
                position = transform.Position;
                scale = transform.Scale;
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