#include "MeshManager.h"

#include "core/Log.h"
#include "rendering/backend/RenderDevice.h"

#include <format>

namespace Wayfinder
{
    IndexElementSize MeshManager::ToIndexElementSize(const MeshIndexFormat format)
    {
        return format == MeshIndexFormat::Uint32 ? IndexElementSize::Uint32 : IndexElementSize::Uint16;
    }

    Result<Mesh> MeshManager::UploadSubmesh(RenderDevice& device, const SubmeshCpuData& submesh)
    {
        if (submesh.VertexBytes.empty() || submesh.IndexBytes.empty() || submesh.VertexCount == 0 || submesh.IndexCount == 0)
        {
            return MakeError("Submesh has empty vertex or index data");
        }

        MeshCreateDesc desc{};
        desc.VertexData = submesh.VertexBytes.data();
        desc.VertexDataSize = static_cast<uint32_t>(submesh.VertexBytes.size());
        desc.VertexCount = submesh.VertexCount;
        desc.IndexData = submesh.IndexBytes.data();
        desc.IndexDataSize = static_cast<uint32_t>(submesh.IndexBytes.size());
        desc.IndexCount = submesh.IndexCount;
        desc.IndexElementType = ToIndexElementSize(submesh.IndexFormat);

        Mesh mesh;
        if (!mesh.Create(device, desc))
        {
            return MakeError("GPU buffer creation failed");
        }

        return mesh;
    }

    bool MeshManager::Initialise(RenderDevice& device)
    {
        m_device = &device;
        m_fallbackMesh = Mesh::CreateTexturedPrimitive(device);
        if (!m_fallbackMesh.IsValid())
        {
            Log::Error(LogRenderer, "MeshManager: Failed to create fallback mesh");
            Shutdown();
            return false;
        }

        Log::Info(LogRenderer, "MeshManager initialised (fallback textured cube)");
        return true;
    }

    void MeshManager::Shutdown()
    {
        for (auto& [id, gpuAsset] : m_cache)
        {
            for (Mesh& submesh : gpuAsset->Submeshes)
            {
                submesh.Destroy();
            }
        }
        m_cache.clear();

        m_fallbackMesh.Destroy();
        m_device = nullptr;
    }

    Result<const MeshAssetGPU*> MeshManager::GetOrLoad(const AssetId& assetId, AssetService& assetService)
    {
        if (!m_device)
        {
            return MakeError("MeshManager not initialised");
        }

        if (const auto it = m_cache.find(assetId); it != m_cache.end())
        {
            return it->second.get();
        }

        std::string error;
        const MeshAsset* asset = assetService.LoadAsset<MeshAsset>(assetId, error);
        if (!asset)
        {
            return MakeError(std::format("Failed to load mesh asset '{}': {}", assetId.ToString(), error));
        }

        if (asset->Submeshes.empty())
        {
            return MakeError(std::format("Mesh asset '{}' has no submeshes", asset->Name));
        }

        auto gpuAsset = std::make_unique<MeshAssetGPU>();
        gpuAsset->Submeshes.reserve(asset->Submeshes.size());
        gpuAsset->MaterialSlots.reserve(asset->Submeshes.size());

        for (uint32_t i = 0; i < asset->Submeshes.size(); ++i)
        {
            auto meshResult = UploadSubmesh(*m_device, asset->Submeshes.at(i));
            if (!meshResult)
            {
                // Destroy any submeshes we already uploaded
                for (Mesh& uploaded : gpuAsset->Submeshes)
                {
                    uploaded.Destroy();
                }
                return MakeError(std::format("GPU upload failed for submesh {} of '{}': {}", i, asset->Name, meshResult.error().GetMessage()));
            }

            gpuAsset->Submeshes.push_back(std::move(*meshResult));
            gpuAsset->MaterialSlots.push_back(asset->Submeshes.at(i).MaterialSlot);
        }

        const auto [it, inserted] = m_cache.emplace(assetId, std::move(gpuAsset));
        assetService.ReleaseMeshGeometryData(assetId);

        return it->second.get();
    }

} // namespace Wayfinder
