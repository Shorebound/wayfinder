#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "RenderIntent.h"
#include "core/Assert.h"
#include "core/Identifiers.h"
#include "core/Types.h"
#include "maths/Bounds.h"
#include "maths/Frustum.h"
#include "rendering/RenderTypes.h"
#include "rendering/backend/GPUHandles.h"
#include "rendering/materials/MaterialParameter.h"
#include "volumes/BlendableEffect.h"

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
     * This is a logical identifier, not a generational handle â€” it is never passed to the GPU directly.
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
        /// Index of the submesh within the mesh asset. Only meaningful for asset meshes.
        uint32_t SubmeshIndex = 0;
    };

    /**
     * @brief CPU-side descriptor used by `RenderResourceCache` to look up or create a material resource.
     *
     * This is a logical identifier, not a generational handle â€” it is never passed to the GPU directly.
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

    /**
     * @brief Render-state overrides — rasterizer configuration, separate from material surface params.
     *
     * @todo Future: blend mode, double-sided, stencil ref, etc.
     */
    struct RenderStateOverrides
    {
        /** @brief Optional fill-mode override (solid, wireframe, or both). */
        std::optional<RenderFillMode> FillMode;
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

        // Generic parameter bag â€” the shader program's declarations determine UBO layout.
        MaterialParameterBlock Parameters;

        // Per-entity overrides applied on top of the asset-loaded parameters.
        MaterialParameterBlock Overrides;
        bool HasOverrides = false;

        RenderStateOverrides StateOverrides;

        // Texture bindings â€” maps slot names to asset IDs (authored in material JSON).
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
        AxisAlignedBounds WorldBounds{};
        BoundingSphere WorldSphere{};
        bool Visible = true;
        RenderGroupId Group = RenderGroups::Main;
        uint8_t SortPriority = 128;
        uint64_t SortKey = 0;
        /**
         * @brief Which view's scene layers this submission targets (must match `FrameLayer::ViewIndex` when set).
         *
         * Unset means the submission does not target a view; `FindSceneLayerForSubmission` rejects such submissions.
         */
        std::optional<size_t> ViewIndex;
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
        BlendableEffectStack PostProcess;

        /// Pre-computed matrices and frustum. Populated by RenderOrchestrator::Prepare().
        Matrix4 ViewMatrix = Matrix4(1.0f);
        Matrix4 ProjectionMatrix = Matrix4(1.0f);
        Frustum ViewFrustum{};
        bool Prepared = false;
    };

    /**
     * @brief Discriminator for `FrameLayer::Kind` â€” scene mesh layers vs debug overlay layers.
     */
    enum class FrameLayerKind
    {
        /** Meshes and scene content for a view. */
        Scene,
        /** Debug draw lists (lines, boxes, grid) for a view. */
        Debug
    };

    /**
     * @brief CPU-side layer record (meshes, debug draw, etc.).
     *
     * Not to be confused with `RenderFeature` graph injectors in `rendering/graph/RenderFeature.h`.
     */
    struct FrameLayer
    {
        FrameLayerId Id = FrameLayerIds::MainScene;
        FrameLayerKind Kind = FrameLayerKind::Scene;
        size_t ViewIndex = 0;
        std::optional<RenderGroupId> SceneGroup;
        std::vector<RenderMeshSubmission> Meshes;
        std::optional<RenderDebugDrawList> DebugDraw;
        bool Enabled = true;

        bool AcceptsSceneSubmission(const RenderMeshSubmission& submission) const
        {
            if (Kind != FrameLayerKind::Scene)
            {
                return false;
            }

            return !SceneGroup || submission.Group == *SceneGroup;
        }
    };

    struct RenderFrame
    {
        std::string SceneName;
        std::filesystem::path AssetRoot;
        std::vector<RenderView> Views;
        std::vector<FrameLayer> Layers;
        std::vector<RenderLightSubmission> Lights;

        size_t AddView(const RenderView& view)
        {
            Views.push_back(view);
            return Views.size() - 1;
        }

        /**
         * @brief Registers a scene `FrameLayer` for a view and render layer (e.g. main vs overlay).
         *
         * @param id Frame layer id (e.g. `FrameLayerIds::MainScene`).
         * @param viewIndex Index into `Views`; must be in range.
         * @param sceneGroup Which `RenderGroupId` this record accepts (see `AcceptsSceneSubmission`).
         * @return Reference to the new record in `Layers`.
         *
         * @warning The returned `FrameLayer&` refers to an element inside `Layers`. It is
         *          invalidated if `Layers` reallocates (for example when pushing more layers). Do not
         *          store this reference across operations that may grow `Layers`; keep an index or id
         *          and resolve via `FindLayer` instead.
         */
        FrameLayer& AddSceneLayer(const FrameLayerId& id, size_t viewIndex, const RenderGroupId& sceneGroup)
        {
            WAYFINDER_ASSERT(viewIndex < Views.size(), "AddSceneLayer: viewIndex out of range for Views");
            for (const auto& existing : Layers)
            {
                if (existing.Id == id && existing.ViewIndex == viewIndex)
                {
                    WAYFINDER_ASSERT(false, "AddSceneLayer: duplicate FrameLayerId for the same viewIndex");
                }
            }
            const std::optional<RenderGroupId> sceneTarget{sceneGroup};
            for (const auto& existing : Layers)
            {
                if (existing.Kind == FrameLayerKind::Scene && existing.ViewIndex == viewIndex && existing.SceneGroup == sceneTarget)
                {
                    WAYFINDER_ASSERT(false, "AddSceneLayer: duplicate scene group registration for the same viewIndex and RenderGroupId");
                }
            }

            FrameLayer layer;
            layer.Id = id;
            layer.Kind = FrameLayerKind::Scene;
            layer.ViewIndex = viewIndex;
            layer.SceneGroup = sceneGroup;
            Layers.push_back(std::move(layer));
            return Layers.back();
        }

        /**
         * @brief Registers a debug `FrameLayer` for a view (lines, boxes, grid).
         *
         * @param id Frame layer id (e.g. `FrameLayerIds::Debug`).
         * @param viewIndex Index into `Views`; must be in range.
         * @return Reference to the new record in `Layers`.
         *
         * @warning The returned `FrameLayer&` refers to an element inside `Layers`. It is
         *          invalidated if `Layers` reallocates (for example when pushing more layers). Do not
         *          store this reference across operations that may grow `Layers`; keep an index or id
         *          and resolve via `FindLayer` instead.
         */
        FrameLayer& AddDebugLayer(const FrameLayerId& id, size_t viewIndex)
        {
            WAYFINDER_ASSERT(viewIndex < Views.size(), "AddDebugLayer: viewIndex out of range for Views");
            for (const auto& existing : Layers)
            {
                if (existing.Id == id && existing.ViewIndex == viewIndex)
                {
                    WAYFINDER_ASSERT(false, "AddDebugLayer: duplicate FrameLayerId for the same viewIndex");
                }
            }

            FrameLayer layer;
            layer.Id = id;
            layer.Kind = FrameLayerKind::Debug;
            layer.ViewIndex = viewIndex;
            layer.DebugDraw = RenderDebugDrawList{};
            Layers.push_back(std::move(layer));
            return Layers.back();
        }

        /**
         * @brief Resolves a layer by id only when that id is unique across the frame.
         *
         * @param id Layer id to find.
         * @return Pointer to the single matching layer, or nullptr if none or if the same id appears on more than one view.
         *
         * If the same id appears on more than one view, returns nullptr â€” use `FindLayer(id, viewIndex)` instead.
         */
        FrameLayer* FindLayer(const FrameLayerId& id)
        {
            FrameLayer* match = nullptr;
            for (auto& layer : Layers)
            {
                if (layer.Id == id)
                {
                    if (match != nullptr)
                    {
                        return nullptr;
                    }
                    match = &layer;
                }
            }

            return match;
        }

        /**
         * @brief Resolves a layer by id only when that id is unique across the frame (const).
         *
         * @param id Layer id to find.
         * @return Pointer to the single matching layer, or nullptr if none or if the same id appears on more than one view.
         */
        const FrameLayer* FindLayer(const FrameLayerId& id) const
        {
            const FrameLayer* match = nullptr;
            for (const auto& layer : Layers)
            {
                if (layer.Id == id)
                {
                    if (match != nullptr)
                    {
                        return nullptr;
                    }
                    match = &layer;
                }
            }

            return match;
        }

        /**
         * @brief Resolves a layer by id and view index.
         *
         * @param id Layer id to find.
         * @param viewIndex View index; must match `FrameLayer::ViewIndex`.
         * @return Pointer to the matching layer, or nullptr.
         */
        FrameLayer* FindLayer(const FrameLayerId& id, size_t viewIndex)
        {
            for (FrameLayer& layer : Layers)
            {
                if (layer.Id == id && layer.ViewIndex == viewIndex)
                {
                    return &layer;
                }
            }

            return nullptr;
        }

        /**
         * @brief Resolves a layer by id and view index (const).
         *
         * @param id Layer id to find.
         * @param viewIndex View index; must match `FrameLayer::ViewIndex`.
         * @return Pointer to the matching layer, or nullptr.
         */
        const FrameLayer* FindLayer(const FrameLayerId& id, size_t viewIndex) const
        {
            for (const FrameLayer& layer : Layers)
            {
                if (layer.Id == id && layer.ViewIndex == viewIndex)
                {
                    return &layer;
                }
            }

            return nullptr;
        }

        /**
         * @brief Finds the scene layer that accepts this mesh submission.
         *
         * @param submission Draw submission; if `ViewIndex` is unset, returns nullptr (cannot resolve).
         * @return Pointer to the accepting scene layer, or nullptr if `ViewIndex` is unset or no layer matches.
         */
        FrameLayer* FindSceneLayerForSubmission(const RenderMeshSubmission& submission)
        {
            return const_cast<FrameLayer*>(std::as_const(*this).FindSceneLayerForSubmission(submission));
        }

        /**
         * @brief Finds the scene layer that accepts this mesh submission (const).
         *
         * @param submission Draw submission; if `ViewIndex` is unset, returns nullptr (cannot resolve).
         * @return Pointer to the accepting scene layer, or nullptr if `ViewIndex` is unset or no layer matches.
         */
        const FrameLayer* FindSceneLayerForSubmission(const RenderMeshSubmission& submission) const
        {
            if (!submission.ViewIndex.has_value())
            {
                return nullptr;
            }

            const size_t submissionViewIndex = *submission.ViewIndex;
            for (const auto& layer : Layers)
            {
                if (layer.ViewIndex == submissionViewIndex && layer.AcceptsSceneSubmission(submission))
                {
                    return &layer;
                }
            }

            return nullptr;
        }
    };
} // namespace Wayfinder
