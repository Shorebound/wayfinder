#pragma once

#include <optional>
#include <string>

#include "core/Identifiers.h"
#include "core/Types.h"
#include "maths/Maths.h"
#include "rendering/graph/RenderIntent.h"
#include "rendering/materials/PostProcessVolume.h"

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

    struct SceneEntityComponent {};

    /// Relationship tag for scene-entity ownership.
    /// Usage: entity.add<SceneOwnership>(sceneTag)
    struct SceneOwnership {};

    struct NameComponent
    {
        std::string Value;

        NameComponent() = default;
        NameComponent(const NameComponent&) = default;
        NameComponent& operator=(const NameComponent&) = default;
        explicit NameComponent(std::string value) : Value(std::move(value)) {}
    };

    struct SceneObjectIdComponent
    {
        SceneObjectId Value;

        SceneObjectIdComponent() = default;
        SceneObjectIdComponent(const SceneObjectIdComponent&) = default;
        SceneObjectIdComponent& operator=(const SceneObjectIdComponent&) = default;
        SceneObjectIdComponent(const SceneObjectId& value) : Value(value) {}
    };

    struct TransformComponent
    {
        Transform Local;

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent& operator=(const TransformComponent&) = default;
        explicit TransformComponent(const Float3& position) : Local({.Position = position}) {}
        explicit TransformComponent(const Transform& transform) : Local(transform) {}

        Matrix4 GetLocalMatrix() const
        {
            return Maths::ComposeTransform(Local);
        }
    };

    struct WorldTransformComponent
    {
        Float3 Position = {0.0f, 0.0f, 0.0f};
        Float3 Scale = {1.0f, 1.0f, 1.0f};
        Matrix4 LocalToWorld = Matrix4(1.0f);

        WorldTransformComponent() = default;
        WorldTransformComponent(const WorldTransformComponent&) = default;
        WorldTransformComponent& operator=(const WorldTransformComponent&) = default;
    };

    struct ActiveCameraStateComponent
    {
        bool IsValid = false;
        Float3 Position = {0.0f, 0.0f, 0.0f};
        Float3 Target = {0.0f, 0.0f, 0.0f};
        Float3 Up = {0.0f, 1.0f, 0.0f};
        float FieldOfView = 45.0f;
        ProjectionMode Projection = ProjectionMode::Perspective;

        ActiveCameraStateComponent() = default;
        ActiveCameraStateComponent(const ActiveCameraStateComponent&) = default;
        ActiveCameraStateComponent& operator=(const ActiveCameraStateComponent&) = default;
    };

    enum class MeshPrimitive
    {
        Cube
    };

    struct MeshComponent
    {
        MeshPrimitive Primitive = MeshPrimitive::Cube;
        Float3 Dimensions = {1.0f, 1.0f, 1.0f};

        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = default;
        MeshComponent& operator=(const MeshComponent&) = default;
    };

    struct MaterialComponent
    {
        std::optional<AssetId> MaterialAssetId;
        Colour BaseColour = Colour::White();
        bool HasBaseColourOverride = false;

        MaterialComponent() = default;
        MaterialComponent(const MaterialComponent&) = default;
        MaterialComponent& operator=(const MaterialComponent&) = default;
    };

    /**
     * @brief Opt-in render-state overrides — controls rasteriser behaviour
     *        independently of material surface properties.
     *
     * Each field is optional: `std::nullopt` means "not overriding this state".
     * This prevents accidental side-effects when adding the component for one
     * override (e.g. CullMode) without intending to change another (Wireframe).
     *
     * @todo Add CullMode, blend overrides, and other rasteriser state fields.
     */
    struct RenderOverrideComponent
    {
        std::optional<bool> Wireframe;

        RenderOverrideComponent() = default;
        RenderOverrideComponent(const RenderOverrideComponent&) = default;
        RenderOverrideComponent& operator=(const RenderOverrideComponent&) = default;
    };

    struct RenderableComponent
    {
        bool Visible = true;
        RenderLayerId Layer = RenderLayers::Main;
        uint8_t SortPriority = 128;

        RenderableComponent() = default;
        RenderableComponent(const RenderableComponent&) = default;
        RenderableComponent& operator=(const RenderableComponent&) = default;
    };

    struct PrefabInstanceComponent
    {
        AssetId SourceAssetId;

        PrefabInstanceComponent() = default;
        PrefabInstanceComponent(const PrefabInstanceComponent&) = default;
        PrefabInstanceComponent& operator=(const PrefabInstanceComponent&) = default;
        PrefabInstanceComponent(const AssetId& assetId) : SourceAssetId(assetId) {}
    };

    struct CameraComponent
    {
        bool Primary = false;
        Float3 Target = {0.0f, 0.0f, 0.0f};
        Float3 Up = {0.0f, 1.0f, 0.0f};
        float FieldOfView = 45.0f;
        ProjectionMode Projection = ProjectionMode::Perspective;

        CameraComponent() = default;
        CameraComponent(const CameraComponent&) = default;
        CameraComponent& operator=(const CameraComponent&) = default;
    };

    struct LightComponent
    {
        LightType Type = LightType::Point;
        Colour Tint = Colour::Yellow();
        float Intensity = 1.0f;
        float Range = 8.0f;
        bool DebugDraw = true;

        LightComponent() = default;
        LightComponent(const LightComponent&) = default;
        LightComponent& operator=(const LightComponent&) = default;
    };
}
