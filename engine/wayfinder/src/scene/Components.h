#pragma once

#include <optional>
#include <string>

#include "../core/Identifiers.h"
#include "../maths/Maths.h"
#include "../rendering/PostProcessVolume.h"
#include "../rendering/RenderIntent.h"
#include "../rendering/RenderTypes.h"

namespace Wayfinder
{
    // Common components for all entities

    enum class ProjectionMode
    {
        Perspective = 0,
        Orthographic = 1
    };

    enum class LightType
    {
        Point,
        Directional
    };

    struct SceneEntityComponent
    {
    };

    /// Relationship tag for scene-entity ownership.
    /// Usage: entity.add<SceneOwnership>(sceneTag)
    struct SceneOwnership
    {
    };

    struct NameComponent
    {
        std::string Value;

        NameComponent() = default;
        NameComponent(const NameComponent&) = default;
        explicit NameComponent(std::string value) : Value(std::move(value)) {}
    };

    struct SceneObjectIdComponent
    {
        SceneObjectId Value;

        SceneObjectIdComponent() = default;
        SceneObjectIdComponent(const SceneObjectIdComponent&) = default;
        SceneObjectIdComponent(const SceneObjectId& value) : Value(value) {}
    };

    struct TransformComponent
    {
        Float3 Position = { 0.0f, 0.0f, 0.0f };
        Float3 Rotation = { 0.0f, 0.0f, 0.0f };
        Float3 Scale = { 1.0f, 1.0f, 1.0f };

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const Float3& position) : Position(position) {}

        Matrix4 GetLocalMatrix() const
        {
            return Math3D::ComposeTransform(Position, Rotation, Scale);
        }
    };

    struct WorldTransformComponent
    {
        Float3 Position = { 0.0f, 0.0f, 0.0f };
        Float3 Scale = { 1.0f, 1.0f, 1.0f };
        Matrix4 LocalToWorld = glm::mat4(1.0f);

        WorldTransformComponent() = default;
        WorldTransformComponent(const WorldTransformComponent&) = default;
    };

    struct ActiveCameraStateComponent
    {
        bool IsValid = false;
        Float3 Position = { 0.0f, 0.0f, 0.0f };
        Float3 Target = { 0.0f, 0.0f, 0.0f };
        Float3 Up = { 0.0f, 1.0f, 0.0f };
        float FieldOfView = 45.0f;
        ProjectionMode Projection = ProjectionMode::Perspective;

        ActiveCameraStateComponent() = default;
        ActiveCameraStateComponent(const ActiveCameraStateComponent&) = default;
    };

    enum class MeshPrimitive
    {
        Cube
    };

    struct MeshComponent
    {
        MeshPrimitive Primitive = MeshPrimitive::Cube;
        Float3 Dimensions = { 1.0f, 1.0f, 1.0f };

        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = default;
    };

    struct MaterialComponent
    {
        std::optional<AssetId> MaterialAssetId;
        Color BaseColor = Color::White();
        bool HasBaseColorOverride = false;
        bool Wireframe = true;
        bool HasWireframeOverride = false;

        MaterialComponent() = default;
        MaterialComponent(const MaterialComponent&) = default;
    };

    struct RenderableComponent
    {
        bool Visible = true;
        RenderLayerId Layer = std::string(RenderLayers::Main);
        uint8_t SortPriority = 128;

        RenderableComponent() = default;
        RenderableComponent(const RenderableComponent&) = default;
    };

    struct PrefabInstanceComponent
    {
        AssetId SourceAssetId;

        PrefabInstanceComponent() = default;
        PrefabInstanceComponent(const PrefabInstanceComponent&) = default;
        PrefabInstanceComponent(const AssetId& assetId) : SourceAssetId(assetId) {}
    };

    struct CameraComponent
    {
        bool Primary = false;
        Float3 Target = { 0.0f, 0.0f, 0.0f };
        Float3 Up = { 0.0f, 1.0f, 0.0f };
        float FieldOfView = 45.0f;
        ProjectionMode Projection = ProjectionMode::Perspective;

        CameraComponent() = default;
        CameraComponent(const CameraComponent&) = default;
    };

    struct LightComponent
    {
        LightType Type = LightType::Point;
        Color Tint = Color::Yellow();
        float Intensity = 1.0f;
        float Range = 8.0f;
        bool DebugDraw = true;

        LightComponent() = default;
        LightComponent(const LightComponent&) = default;
    };
}
