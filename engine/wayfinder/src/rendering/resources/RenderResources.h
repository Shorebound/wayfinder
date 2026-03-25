#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>

#include "assets/AssetService.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/materials/Material.h"
#include "rendering/materials/ShaderProgram.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class MeshManager;
    class TextureManager;
    class Mesh;

    struct RenderMeshResource
    {
        RenderMeshRef Ref{};
        RenderGeometry Geometry{};
        /// Populated for asset meshes when a mesh manager is wired into the cache.
        const Mesh* GpuMesh = nullptr;
    };

    struct RenderMaterialResource
    {
        RenderMaterialRef Ref{};
        RenderMaterialBinding Binding{};
        bool IsLoadedFromAsset = false;
    };

    class WAYFINDER_API RenderResourceCache
    {
    public:
        void SetAssetService(const std::shared_ptr<AssetService>& assetService);

        /** @brief Set the texture manager used for GPU texture lookups.
         *  @param textureManager Non-owning pointer; the caller must ensure the
         *         TextureManager outlives this cache. */
        void SetTextureManager(TextureManager* textureManager);

        /** @brief Non-owning pointer — must outlive this cache. */
        void SetMeshManager(MeshManager* meshManager);

        /** @brief Set the shader program registry used for material resolution.
         *  @param programs Non-owning pointer; the caller must ensure the
         *         ShaderProgramRegistry outlives this cache. */
        void SetProgramRegistry(const ShaderProgramRegistry* programs);
        void PrepareFrame(RenderFrame& frame);

        const RenderMeshResource& ResolveMesh(const RenderMeshSubmission& submission);

    private:
        RenderMaterialBinding PrepareMaterialBinding(const RenderMaterialBinding& binding);
        RenderMaterialResource CreateMaterialResource(const RenderMaterialBinding& binding);
        void ResolveTextureBindings(RenderMaterialBinding& binding);

        std::shared_ptr<AssetService> m_assetService;
        TextureManager* m_textureManager = nullptr;
        MeshManager* m_meshManager = nullptr;
        const ShaderProgramRegistry* m_programs = nullptr;
        std::unordered_map<uint64_t, RenderMeshResource> m_meshesByKey;
        std::unordered_map<uint64_t, RenderMaterialResource> m_materialsByKey;
    };
} // namespace Wayfinder