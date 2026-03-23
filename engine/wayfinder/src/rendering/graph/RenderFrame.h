#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "RenderIntent.h"
#include "core/Identifiers.h"
#include "core/Types.h"
#include "rendering/RenderTypes.h"
#include "rendering/backend/GPUHandles.h"
#include "rendering/materials/MaterialParameter.h"
#include "rendering/materials/PostProcessVolume.h"

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

    /**
     * @brief CPU-side descriptor used by `RenderResourceCache` to look up or create a mesh resource.
     *
     * This is a logical identifier, not a generational handle — it is never passed to the GPU directly.
     * For the type-safe generational handle see `RenderMeshHandle` in `GPUHandles.h`.
     *
     * @param Origin  Whether this mesh comes from the built-in geometry library or an asset file.
     * @param AssetId Asset identifier, populated when `Origin` is `RenderResourceOrigin::Asset`.
     * @param StableKey Deterministic 64-bit key derived from the mesh identity, used as the cache
     *                  lookup key in `RenderResourceCache::m_meshesByKey`.
     */
    struct RenderMeshRef
    {
        RenderResourceOrigin Origin = RenderResourceOrigin::BuiltIn;
        std::optional<AssetId> AssetId;
        uint64_t StableKey = 0;
    };

    /**
     * @brief CPU-side descriptor used by `RenderResourceCache` to look up or create a material resource.
     *
     * This is a logical identifier, not a generational handle — it is never passed to the GPU directly.
     * For the type-safe generational handle see `RenderMaterialHandle` in `GPUHandles.h`.
     *
     * @param Origin  Whether this material comes from the built-in material library or an asset file.
     * @param AssetId Asset identifier, populated when `Origin` is `RenderResourceOrigin::Asset`.
     * @param StableKey Deterministic 64-bit key derived from the material identity, used as the cache
     *                  lookup key in `RenderResourceCache::m_materialsByKey`.
     */
    struct RenderMaterialRef
    {
        RenderResourceOrigin Origin = RenderResourceOrigin::BuiltIn;
        std::optional<AssetId> AssetId;
        uint64_t StableKey = 0;
    };

    // Render-state overrides — rasterizer configuration, separate from material surface params.
    struct RenderStateOverrides
    {
        std::optional<RenderFillMode> FillMode;
        // Future: blend mode, double-sided, stencil ref, etc.
    };

    /// Maps named texture slots (e.g. "diffuse") to texture asset IDs.
    /// Resolved to GPU handles at render time by the TextureManager.
    struct TextureBindingSet
    {
        std::unordered_map<std::string, AssetId> Slots;

        bool HasSlot(const std::string& name) const
        {
            return Slots.contains(name);
        }
    };

    /// Resolved GPU handles for texture bindings (filled by RenderResourceCache).
    struct ResolvedTextureBinding
    {
        GPUTextureHandle Texture{};
        GPUSamplerHandle Sampler{};
        uint32_t Slot = 0;
    };

    struct RenderMaterialBinding
    {
        RenderMaterialRef Ref{};
        RenderMaterialDomain Domain = RenderMaterialDomain::Surface;
        std::string ShaderName = "unlit";

        // Generic parameter bag — the shader program's declarations determine UBO layout.
        MaterialParameterBlock Parameters;

        // Per-entity overrides applied on top of the asset-loaded parameters.
        MaterialParameterBlock Overrides;
        bool HasOverrides = false;

        RenderStateOverrides StateOverrides;

        // Texture bindings — maps slot names to asset IDs (authored in material JSON).
        TextureBindingSet Textures;

        // Resolved GPU handles for texture bindings (populated at render time).
        std::vector<ResolvedTextureBinding> ResolvedTextures;
    };

    struct RenderMeshSubmission
    {
        RenderMeshRef Mesh{};
        Matrix4 LocalToWorld = Matrix4(1.0f);
        RenderGeometry Geometry{};
        RenderMaterialBinding Material{};
        bool Visible = true;
        RenderLayerId Layer = RenderLayers::Main;
        uint8_t SortPriority = 128;
        uint64_t SortKey = 0;
    };

    struct RenderLightSubmission
    {
        RenderLightType Type = RenderLightType::Point;
        Float3 Position{0.0f, 0.0f, 0.0f};
        Float3 Direction{0.0f, 0.0f, -1.0f};
        Colour Tint = Colour::White();
        float Intensity = 1.0f;
        float Range = 1.0f;
        bool DebugDraw = false;
    };

    struct RenderDebugLine
    {
        Float3 Start{0.0f, 0.0f, 0.0f};
        Float3 End{0.0f, 0.0f, 0.0f};
        Colour Tint = Colour::White();
    };

    struct RenderDebugBox
    {
        Matrix4 LocalToWorld = Matrix4(1.0f);
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
        Colour ClearColour = Colour::White();
        bool IsPrimary = true;
        PostProcessStack PostProcess;
    };

    enum class RenderPassKind
    {
        Scene,
        Debug
    };

    struct RenderPass
    {
        RenderPassId Id = RenderPassIds::MainScene;
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

        RenderPass& AddScenePass(const RenderPassId& id, size_t viewIndex, const RenderLayerId& sceneLayer)
        {
            RenderPass pass;
            pass.Id = id;
            pass.Kind = RenderPassKind::Scene;
            pass.ViewIndex = viewIndex;
            pass.SceneLayer = sceneLayer;
            Passes.push_back(std::move(pass));
            return Passes.back();
        }

        RenderPass& AddDebugPass(const RenderPassId& id, size_t viewIndex)
        {
            RenderPass pass;
            pass.Id = id;
            pass.Kind = RenderPassKind::Debug;
            pass.ViewIndex = viewIndex;
            pass.DebugDraw = RenderDebugDrawList{};
            Passes.push_back(std::move(pass));
            return Passes.back();
        }

        RenderPass* FindPass(const RenderPassId& id)
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

        const RenderPass* FindPass(const RenderPassId& id) const
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