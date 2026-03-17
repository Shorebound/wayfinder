#include "SceneRenderExtractor.h"

#include "../scene/Components.h"
#include "../scene/Scene.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace
{
    constexpr uint64_t kBuiltInBoxMeshKey = 1;

    Wayfinder::Float3 ToFloat3(const Vector3& value)
    {
        return {value.x, value.y, value.z};
    }

    Wayfinder::Matrix4 ToMatrix4(const Matrix& matrix)
    {
        Wayfinder::Matrix4 result;
        result.m0 = matrix.m0;
        result.m4 = matrix.m4;
        result.m8 = matrix.m8;
        result.m12 = matrix.m12;
        result.m1 = matrix.m1;
        result.m5 = matrix.m5;
        result.m9 = matrix.m9;
        result.m13 = matrix.m13;
        result.m2 = matrix.m2;
        result.m6 = matrix.m6;
        result.m10 = matrix.m10;
        result.m14 = matrix.m14;
        result.m3 = matrix.m3;
        result.m7 = matrix.m7;
        result.m11 = matrix.m11;
        result.m15 = matrix.m15;
        return result;
    }

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

    uint64_t BuildSortKey(const std::optional<Wayfinder::AssetId>& materialAssetId, const Wayfinder::RenderGeometryType geometryType)
    {
        const uint64_t geometryBits = static_cast<uint64_t>(geometryType) & 0xFFull;
        const uint64_t materialBits = materialAssetId ? (MakeStableKey(*materialAssetId) & 0xFFFFFFFFFFFFFF00ull) : 0ull;
        return materialBits | geometryBits;
    }
}

namespace Wayfinder
{
    RenderFrame SceneRenderExtractor::Extract(const Scene& scene) const
    {
        RenderFrame frame;
        frame.SceneName = scene.GetName();
        frame.AssetRoot = scene.GetAssetRoot();
        frame.Debug.ShowWorldGrid = true;

        if (scene.GetWorld().has<ActiveCameraStateComponent>())
        {
            const ActiveCameraStateComponent& activeCamera = scene.GetWorld().get<ActiveCameraStateComponent>();
            if (activeCamera.IsValid)
            {
                RenderView view;
                view.CameraState.Position = ToFloat3(activeCamera.Position);
                view.CameraState.Target = ToFloat3(activeCamera.Target);
                view.CameraState.Up = ToFloat3(activeCamera.Up);
                view.CameraState.FOV = activeCamera.FieldOfView;
                view.CameraState.ProjectionType = static_cast<int>(activeCamera.Projection);
                frame.Views.push_back(view);
            }
        }

        scene.GetWorld().each([&frame](flecs::entity entityHandle, const TransformComponent& transform, const MeshComponent& mesh)
        {
            RenderMeshSubmission submission;
            submission.Mesh.Origin = RenderResourceOrigin::BuiltIn;
            submission.Mesh.StableKey = kBuiltInBoxMeshKey;
            submission.Geometry.Type = RenderGeometryType::Box;
            submission.Geometry.Dimensions = ToFloat3(mesh.Dimensions);
            submission.Material.Handle.Origin = RenderResourceOrigin::BuiltIn;
            submission.Material.Handle.StableKey = mesh.Wireframe ? 2ull : 1ull;
            submission.Material.Domain = RenderMaterialDomain::Surface;
            submission.Material.BaseColor = mesh.Albedo;
            submission.Material.HasBaseColorOverride = true;
            submission.Material.WireframeColor = Color::DarkGray();
            submission.Material.HasWireframeColorOverride = true;
            submission.Material.FillMode = mesh.Wireframe ? RenderFillMode::SolidAndWireframe : RenderFillMode::Solid;
            submission.Material.HasFillModeOverride = true;

            Matrix localToWorld = transform.GetLocalMatrix();

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
                    submission.Material.Handle.Origin = RenderResourceOrigin::Asset;
                    submission.Material.Handle.AssetId = material.MaterialAssetId;
                    submission.Material.Handle.StableKey = MakeStableKey(*material.MaterialAssetId);
                }

                if (material.HasBaseColorOverride || !material.MaterialAssetId)
                {
                    submission.Material.BaseColor = material.BaseColor;
                    submission.Material.HasBaseColorOverride = true;
                }

                if (material.HasWireframeOverride || !material.MaterialAssetId)
                {
                    submission.Material.FillMode = material.Wireframe ? RenderFillMode::SolidAndWireframe : RenderFillMode::Solid;
                    submission.Material.HasFillModeOverride = true;
                }
            }

            submission.LocalToWorld = ToMatrix4(localToWorld);
            submission.SortKey = BuildSortKey(submission.Material.Handle.AssetId, submission.Geometry.Type);
            frame.Meshes.push_back(submission);
        });

        scene.GetWorld().each([&frame](flecs::entity entityHandle, const TransformComponent& transform, const LightComponent& light)
        {
            Matrix localToWorld = transform.GetLocalMatrix();
            Vector3 position = transform.Position;
            if (entityHandle.has<WorldTransformComponent>())
            {
                const auto& worldTransform = entityHandle.get<WorldTransformComponent>();
                localToWorld = worldTransform.LocalToWorld;
                position = worldTransform.Position;
            }

            const Vector3 direction = Vector3Normalize(Math3D::TransformDirection(localToWorld, {0.0f, 0.0f, -1.0f}));

            RenderLightSubmission submission;
            submission.Type = light.Type == LightType::Directional ? RenderLightType::Directional : RenderLightType::Point;
            submission.Position = ToFloat3(position);
            submission.Direction = ToFloat3(direction);
            submission.Tint = light.Tint;
            submission.Intensity = light.Intensity;
            submission.Range = light.Range;
            submission.DebugDraw = light.DebugDraw;
            frame.Lights.push_back(submission);

            if (light.DebugDraw)
            {
                const float debugSize = light.Type == LightType::Directional ? 0.6f : 0.3f;
                Matrix debugTransform = MatrixMultiply(MatrixScale(debugSize, debugSize, debugSize), MatrixTranslate(position.x, position.y, position.z));

                RenderDebugBox debugBox;
                debugBox.LocalToWorld = ToMatrix4(debugTransform);
                debugBox.Dimensions = {1.0f, 1.0f, 1.0f};
                debugBox.Material.Handle.Origin = RenderResourceOrigin::BuiltIn;
                debugBox.Material.Handle.StableKey = 100ull;
                debugBox.Material.Domain = RenderMaterialDomain::Debug;
                debugBox.Material.BaseColor = light.Tint;
                debugBox.Material.HasBaseColorOverride = true;
                debugBox.Material.WireframeColor = Color::DarkGray();
                debugBox.Material.HasWireframeColorOverride = true;
                debugBox.Material.FillMode = RenderFillMode::SolidAndWireframe;
                debugBox.Material.HasFillModeOverride = true;
                frame.Debug.Boxes.push_back(debugBox);

                if (light.Type == LightType::Directional)
                {
                    const Vector3 lineEnd = Vector3Add(position, Vector3Scale(direction, 1.5f));
                    RenderDebugLine debugLine;
                    debugLine.Start = ToFloat3(position);
                    debugLine.End = ToFloat3(lineEnd);
                    debugLine.Color = light.Tint;
                    frame.Debug.Lines.push_back(debugLine);
                }
            }
        });

        return frame;
    }
} // namespace Wayfinder