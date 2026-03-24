#pragma once

#include "assets/AssetService.h"
#include "assets/MeshFormat.h"
#include "rendering/mesh/Mesh.h"

#include <unordered_map>

#include "wayfinder_exports.h"

namespace Wayfinder
{
    class RenderDevice;

    /**
     * @brief GPU mesh cache for mesh assets — mirrors TextureManager.
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
         * @brief Upload submesh 0 from the mesh asset, or return the fallback cube on failure.
         */
        Mesh* GetOrLoad(const AssetId& assetId, AssetService& assetService);

        Mesh& GetFallbackMesh()
        {
            return m_fallbackMesh;
        }

    private:
        static IndexElementSize ToIndexElementSize(MeshIndexFormat format);
        static bool UploadSubmesh0(RenderDevice& device, const MeshAsset& asset, Mesh& outMesh);

        RenderDevice* m_device = nullptr;
        std::unordered_map<AssetId, Mesh> m_cache;
        Mesh m_fallbackMesh;
    };

} // namespace Wayfinder
