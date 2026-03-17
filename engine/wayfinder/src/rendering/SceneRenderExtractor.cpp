#include "SceneRenderExtractor.h"

#include "../scene/Components.h"
#include "../scene/Scene.h"

namespace
{
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
}

namespace Wayfinder
{
    RenderFrame SceneRenderExtractor::Extract(const Scene& scene) const
    {
        RenderFrame frame;
        frame.SceneName = scene.GetName();

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
            submission.Geometry.Type = RenderGeometryType::Box;
            submission.Geometry.Dimensions = ToFloat3(mesh.Dimensions);
            submission.Material.Tint = mesh.Albedo;
            submission.Material.Wireframe = mesh.Wireframe;

            Matrix localToWorld = transform.GetLocalMatrix();

            if (entityHandle.has<WorldTransformComponent>())
            {
                const auto& worldTransform = entityHandle.get<WorldTransformComponent>();
                localToWorld = worldTransform.LocalToWorld;
            }

            if (entityHandle.has<MaterialComponent>())
            {
                const auto& material = entityHandle.get<MaterialComponent>();
                submission.Material.MaterialAssetId = material.MaterialAssetId;
                submission.Material.Tint = material.BaseColor;
                submission.Material.Wireframe = material.Wireframe;
            }

            submission.LocalToWorld = ToMatrix4(localToWorld);
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
                debugBox.FillColor = light.Tint;
                debugBox.WireColor = Color::DarkGray();
                debugBox.Solid = true;
                debugBox.Wireframe = true;
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