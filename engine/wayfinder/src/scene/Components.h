#pragma once

#include <optional>
#include <string>

#include "raylib.h"
#include "raymath.h"

#include "../core/Identifiers.h"
#include "../maths/Maths.h"
#include "../rendering/RenderAPI.h"

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
        Vector3 Position = { 0.0f, 0.0f, 0.0f };
        Vector3 Rotation = { 0.0f, 0.0f, 0.0f };
        Vector3 Scale = { 1.0f, 1.0f, 1.0f };

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const Vector3& position) : Position(position) {}

        Matrix GetLocalMatrix() const
        {
            return Math3D::ComposeTransform(Position, Rotation, Scale);
        }
    };

    struct WorldTransformComponent
    {
        Vector3 Position = { 0.0f, 0.0f, 0.0f };
        Vector3 Scale = { 1.0f, 1.0f, 1.0f };
        Matrix LocalToWorld = MatrixIdentity();

        WorldTransformComponent() = default;
        WorldTransformComponent(const WorldTransformComponent&) = default;
    };

    struct ActiveCameraStateComponent
    {
        bool IsValid = false;
        Vector3 Position = { 0.0f, 0.0f, 0.0f };
        Vector3 Target = { 0.0f, 0.0f, 0.0f };
        Vector3 Up = { 0.0f, 1.0f, 0.0f };
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
        Vector3 Dimensions = { 1.0f, 1.0f, 1.0f };
        Color Albedo = Color::Red();
        bool Wireframe = true;

        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = default;
    };

    struct MaterialComponent
    {
        std::optional<AssetId> MaterialAssetId;
        Color BaseColor = Color::White();
        bool Wireframe = true;

        MaterialComponent() = default;
        MaterialComponent(const MaterialComponent&) = default;
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
        Vector3 Target = { 0.0f, 0.0f, 0.0f };
        Vector3 Up = { 0.0f, 1.0f, 0.0f };
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
