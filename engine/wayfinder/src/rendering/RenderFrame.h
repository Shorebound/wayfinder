#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../core/Identifiers.h"
#include "RenderIntent.h"
#include "RenderAPI.h"

namespace Wayfinder
{
    enum class RenderResourceOrigin
    {
        BuiltIn,
        Asset
    };

    enum class RenderGeometryType
    {
        Box
    };

    enum class RenderMaterialDomain
    {
        Surface,
        Debug
    };

    enum class RenderFillMode
    {
        Solid,
        Wireframe,
        SolidAndWireframe
    };

    enum class RenderLightType
    {
        Point,
        Directional
    };

    struct RenderGeometry
    {
        RenderGeometryType Type = RenderGeometryType::Box;
        Float3 Dimensions{1.0f, 1.0f, 1.0f};
    };

    struct RenderMeshHandle
    {
        RenderResourceOrigin Origin = RenderResourceOrigin::BuiltIn;
        std::optional<AssetId> AssetId;
        uint64_t StableKey = 0;
    };

    struct RenderMaterialHandle
    {
        RenderResourceOrigin Origin = RenderResourceOrigin::BuiltIn;
        std::optional<AssetId> AssetId;
        uint64_t StableKey = 0;
    };

    struct RenderMaterialBinding
    {
        RenderMaterialHandle Handle{};
        RenderMaterialDomain Domain = RenderMaterialDomain::Surface;
        Color BaseColor = Color::White();
        bool HasBaseColorOverride = false;
        Color WireframeColor = Color::DarkGray();
        bool HasWireframeColorOverride = false;
        RenderFillMode FillMode = RenderFillMode::SolidAndWireframe;
        bool HasFillModeOverride = false;
    };

    struct RenderMeshSubmission
    {
        RenderMeshHandle Mesh{};
        Matrix4 LocalToWorld = Matrix4::Identity();
        RenderGeometry Geometry{};
        RenderMaterialBinding Material{};
        bool Visible = true;
        RenderSceneLayer Layer = RenderSceneLayer::Main;
        uint8_t SortPriority = 128;
        uint64_t SortKey = 0;
    };

    struct RenderLightSubmission
    {
        RenderLightType Type = RenderLightType::Point;
        Float3 Position{0.0f, 0.0f, 0.0f};
        Float3 Direction{0.0f, 0.0f, -1.0f};
        Color Tint = Color::White();
        float Intensity = 1.0f;
        float Range = 1.0f;
        bool DebugDraw = false;
    };

    struct RenderDebugLine
    {
        Float3 Start{0.0f, 0.0f, 0.0f};
        Float3 End{0.0f, 0.0f, 0.0f};
        Color Color = Color::White();
    };

    struct RenderDebugBox
    {
        Matrix4 LocalToWorld = Matrix4::Identity();
        Float3 Dimensions{1.0f, 1.0f, 1.0f};
        RenderMaterialBinding Material{};
    };

    struct RenderDebugDrawList
    {
        bool ShowWorldGrid = false;
        int WorldGridSlices = 100;
        float WorldGridSpacing = 1.0f;
        std::vector<RenderDebugLine> Lines;
        std::vector<RenderDebugBox> Boxes;
    };

    struct RenderView
    {
        Camera CameraState{};
        Color ClearColor = Color::White();
        bool IsPrimary = true;
    };

    struct RenderFrame
    {
        std::string SceneName;
        std::filesystem::path AssetRoot;
        std::vector<RenderView> Views;
        std::vector<RenderMeshSubmission> Meshes;
        std::vector<RenderLightSubmission> Lights;
        RenderDebugDrawList Debug;
    };
} // namespace Wayfinder