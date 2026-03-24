#include "MeshManager.h"

#include "core/Log.h"
#include "rendering/backend/RenderDevice.h"

namespace Wayfinder
{
    namespace
    {
        constexpr uint32_t K_SUBMESH_INDEX = 0u;
    }

    IndexElementSize MeshManager::ToIndexElementSize(const MeshIndexFormat format)
    {
        return format == MeshIndexFormat::Uint32 ? IndexElementSize::Uint32 : IndexElementSize::Uint16;
    }

    bool MeshManager::UploadSubmesh0(RenderDevice& device, const MeshAsset& asset, Mesh& outMesh)
    {
        if (asset.Submeshes.empty())
        {
            return false;
        }

        const SubmeshCpuData& sm = asset.Submeshes[K_SUBMESH_INDEX];
        if (sm.VertexBytes.empty() || sm.IndexBytes.empty() || sm.VertexCount == 0 || sm.IndexCount == 0)
        {
            return false;
        }

        MeshCreateDesc desc{};
        desc.VertexData = sm.VertexBytes.data();
        desc.VertexDataSize = static_cast<uint32_t>(sm.VertexBytes.size());
        desc.VertexCount = sm.VertexCount;
        desc.IndexData = sm.IndexBytes.data();
        desc.IndexDataSize = static_cast<uint32_t>(sm.IndexBytes.size());
        desc.IndexCount = sm.IndexCount;
        desc.IndexElementType = ToIndexElementSize(sm.IndexFormat);

        return outMesh.Create(device, desc);
    }

    bool MeshManager::Initialise(RenderDevice& device)
    {
        m_device = &device;
        m_fallbackMesh = Mesh::CreateTexturedPrimitive(device);
        if (!m_fallbackMesh.IsValid())
        {
            WAYFINDER_ERROR(LogRenderer, "MeshManager: Failed to create fallback mesh");
            Shutdown();
            return false;
        }

        WAYFINDER_INFO(LogRenderer, "MeshManager initialised (fallback textured cube)");
        return true;
    }

    void MeshManager::Shutdown()
    {
        for (auto& [id, mesh] : m_cache)
        {
            mesh.Destroy();
        }
        m_cache.clear();

        m_fallbackMesh.Destroy();
        m_device = nullptr;
    }

    Mesh* MeshManager::GetOrLoad(const AssetId& assetId, AssetService& assetService)
    {
        if (!m_device)
        {
            WAYFINDER_WARNING(LogRenderer, "MeshManager::GetOrLoad called without a valid device");
            return &m_fallbackMesh;
        }

        if (const auto it = m_cache.find(assetId); it != m_cache.end())
        {
            return &it->second;
        }

        std::string error;
        const MeshAsset* asset = assetService.LoadAsset<MeshAsset>(assetId, error);
        if (!asset || asset->Submeshes.empty())
        {
            WAYFINDER_WARNING(LogRenderer, "MeshManager: Failed to load mesh asset '{}': {}", assetId.ToString(), error);
            return &m_fallbackMesh;
        }

        Mesh mesh;
        if (!UploadSubmesh0(*m_device, *asset, mesh))
        {
            WAYFINDER_WARNING(LogRenderer, "MeshManager: GPU upload failed for mesh '{}', using fallback", asset->Name);
            return &m_fallbackMesh;
        }

        const auto [it, inserted] = m_cache.emplace(assetId, std::move(mesh));
        assetService.ReleaseMeshGeometryData(assetId);

        return &it->second;
    }

} // namespace Wayfinder
