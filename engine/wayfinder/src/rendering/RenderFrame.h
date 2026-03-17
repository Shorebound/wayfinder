#pragma once

#include <filesystem>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
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
        RenderLayerId Layer = std::string(RenderLayers::Main);
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

    enum class RenderPassKind
    {
        Scene,
        Debug
    };

    struct RenderPass
    {
        RenderPassId Id = std::string(RenderPassIds::MainScene);
        RenderPassKind Kind = RenderPassKind::Scene;
        size_t ViewIndex = 0;
        std::optional<RenderLayerId> SceneLayer;
        std::vector<RenderMeshSubmission> Meshes;
        std::optional<RenderDebugDrawList> DebugDraw;
        bool Enabled = true;

        bool AcceptsSceneSubmission(const RenderMeshSubmission& submission) const
        {
            if (Kind != RenderPassKind::Scene)
            {
                return false;
            }

            return !SceneLayer || submission.Layer == *SceneLayer;
        }
    };

    struct RenderFrame
    {
        std::string SceneName;
        std::filesystem::path AssetRoot;
        std::vector<RenderView> Views;
        std::vector<RenderPass> Passes;
        std::vector<RenderLightSubmission> Lights;

        size_t AddView(const RenderView& view)
        {
            Views.push_back(view);
            return Views.size() - 1;
        }

        RenderPass& AddScenePass(std::string_view id, size_t viewIndex, std::string_view sceneLayer)
        {
            RenderPass pass;
            pass.Id = std::string(id);
            pass.Kind = RenderPassKind::Scene;
            pass.ViewIndex = viewIndex;
            pass.SceneLayer = std::string(sceneLayer);
            Passes.push_back(std::move(pass));
            return Passes.back();
        }

        RenderPass& AddDebugPass(std::string_view id, size_t viewIndex)
        {
            RenderPass pass;
            pass.Id = std::string(id);
            pass.Kind = RenderPassKind::Debug;
            pass.ViewIndex = viewIndex;
            pass.DebugDraw = RenderDebugDrawList{};
            Passes.push_back(std::move(pass));
            return Passes.back();
        }

        RenderPass* FindPass(std::string_view id)
        {
            for (RenderPass& pass : Passes)
            {
                if (pass.Id == id)
                {
                    return &pass;
                }
            }

            return nullptr;
        }

        const RenderPass* FindPass(std::string_view id) const
        {
            for (const RenderPass& pass : Passes)
            {
                if (pass.Id == id)
                {
                    return &pass;
                }
            }

            return nullptr;
        }

        RenderPass* FindScenePassForSubmission(const RenderMeshSubmission& submission, size_t viewIndex)
        {
            for (RenderPass& pass : Passes)
            {
                if (pass.ViewIndex == viewIndex && pass.AcceptsSceneSubmission(submission))
                {
                    return &pass;
                }
            }

            return nullptr;
        }
    };
} // namespace Wayfinder