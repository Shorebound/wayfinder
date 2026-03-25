#pragma once

#include "assets/AssetService.h"
#include "assets/MeshFormat.h"
#include "core/Result.h"
#include "rendering/mesh/Mesh.h"

#include <memory>
#include <unordered_map>
#include <vector>

#include "wayfinder_exports.h"

namespace Wayfinder
{
    class RenderDevice;

    /**
     * @brief GPU-side representation of a mesh asset — one GPU mesh per submesh.
     *
     * Owns the GPU buffers for every submesh in the source asset.
     * Material slot indices mirror the submesh table in the .wfmesh binary.
     */
    struct MeshAssetGPU
    {
        std::vector<Mesh> Submeshes;
        std::vector<uint32_t> MaterialSlots;
    };

    /**
     * @brief GPU mesh cache for mesh assets — mirrors TextureManager.
     *
     * Loads mesh assets from the AssetService, uploads all submeshes to the GPU,
     * and caches the result keyed by AssetId. Cached entries use `unique_ptr`
     * for pointer stability across cache rehashes.
     */
    class WAYFINDER_API MeshManager
    {
    public:
        MeshManager() = default;
        ~MeshManager() = default;

        MeshManager(const MeshManager&) = delete;
        MeshManager& operator=(const MeshManager&) = delete;
        MeshManager(MeshManager&&) = delete;
        MeshManager& operator=(MeshManager&&) = delete;

        bool Initialise(RenderDevice& device);
        void Shutdown();

        /**
         * @brief Upload all submeshes from the mesh asset, or return an error.
         *
         * On success the returned pointer is stable for the lifetime of the cache entry.
         * On failure the error describes what went wrong — the caller decides how to recover.
         */
        Result<const MeshAssetGPU*> GetOrLoad(const AssetId& assetId, AssetService& assetService);

        Mesh& GetFallbackMesh()
        {
            return m_fallbackMesh;
        }

    private:
        static IndexElementSize ToIndexElementSize(MeshIndexFormat format);
        static Result<Mesh> UploadSubmesh(RenderDevice& device, const SubmeshCpuData& submesh);

        RenderDevice* m_device = nullptr;
        std::unordered_map<AssetId, std::unique_ptr<MeshAssetGPU>> m_cache;
        Mesh m_fallbackMesh;
    };

} // namespace Wayfinder
