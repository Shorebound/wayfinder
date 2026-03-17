#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../core/Identifiers.h"
#include "RenderAPI.h"

namespace Wayfinder
{
    enum class RenderGeometryType
    {
        Box
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

    struct RenderMaterialState
    {
        std::optional<AssetId> MaterialAssetId;
        Color Tint = Color::White();
        bool Wireframe = true;
    };

    struct RenderMeshSubmission
    {
        Matrix4 LocalToWorld = Matrix4::Identity();
        RenderGeometry Geometry{};
        RenderMaterialState Material{};
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
        Color FillColor = Color::White();
        Color WireColor = Color::DarkGray();
        bool Solid = true;
        bool Wireframe = true;
    };

    struct RenderDebugDrawList
    {
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
        std::vector<RenderView> Views;
        std::vector<RenderMeshSubmission> Meshes;
        std::vector<RenderLightSubmission> Lights;
        RenderDebugDrawList Debug;
    };
} // namespace Wayfinder