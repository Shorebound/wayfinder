#include "RenderResources.h"

#include "Material.h"

#include <filesystem>

namespace Wayfinder
{
    void RenderResourceCache::SetAssetService(const std::shared_ptr<AssetService>& assetService)
    {
        if (m_assetService == assetService)
        {
            return;
        }

        m_assetService = assetService;
        m_materialsByKey.clear();
    }

    void RenderResourceCache::PrepareFrame(RenderFrame& frame)
    {
        for (RenderPass& pass : frame.Passes)
        {
            for (RenderMeshSubmission& mesh : pass.Meshes)
            {
                mesh.Material = PrepareMaterialBinding(mesh.Material);
            }

            if (!pass.DebugDraw)
            {
                continue;
            }

            for (RenderDebugBox& debugBox : pass.DebugDraw->Boxes)
            {
                debugBox.Material = PrepareMaterialBinding(debugBox.Material);
            }
        }
    }

    const RenderMeshResource& RenderResourceCache::ResolveMesh(const RenderMeshSubmission& submission)
    {
        auto existing = m_meshesByKey.find(submission.Mesh.StableKey);
        if (existing != m_meshesByKey.end())
        {
            return existing->second;
        }

        RenderMeshResource resource;
        resource.Handle = submission.Mesh;
        resource.Geometry = submission.Geometry;
        const auto [it, inserted] = m_meshesByKey.emplace(submission.Mesh.StableKey, std::move(resource));
        return it->second;
    }

    RenderMaterialBinding RenderResourceCache::PrepareMaterialBinding(const RenderMaterialBinding& binding)
    {
        if (binding.Handle.Origin != RenderResourceOrigin::Asset)
        {
            return binding;
        }

        auto existing = m_materialsByKey.find(binding.Handle.StableKey);
        if (existing == m_materialsByKey.end())
        {
            existing = m_materialsByKey.emplace(binding.Handle.StableKey, CreateMaterialResource(binding)).first;
        }

        RenderMaterialBinding resolved = existing->second.Binding;
        resolved.Handle = binding.Handle;
        resolved.Domain = binding.Domain;

        if (binding.HasBaseColorOverride)
        {
            resolved.BaseColor = binding.BaseColor;
        }

        if (binding.HasWireframeColorOverride)
        {
            resolved.WireframeColor = binding.WireframeColor;
        }

        if (binding.HasFillModeOverride)
        {
            resolved.FillMode = binding.FillMode;
        }

        return resolved;
    }

    RenderMaterialResource RenderResourceCache::CreateMaterialResource(const RenderMaterialBinding& binding)
    {
        RenderMaterialResource resource;
        resource.Handle = binding.Handle;
        resource.Binding = binding;

        if (binding.Handle.Origin != RenderResourceOrigin::Asset || !binding.Handle.AssetId || !m_assetService)
        {
            return resource;
        }

        std::string error;
        const MaterialAsset* materialAsset = m_assetService->LoadMaterialAsset(*binding.Handle.AssetId, error);
        if (!materialAsset)
        {
            return resource;
        }

        resource.IsLoadedFromAsset = true;
        resource.Binding.ShaderName = materialAsset->ShaderName;
        resource.Binding.BaseColor = materialAsset->BaseColor;
        resource.Binding.HasBaseColorOverride = false;
        resource.Binding.WireframeColor = binding.WireframeColor;
        resource.Binding.FillMode = materialAsset->Wireframe ? RenderFillMode::SolidAndWireframe : RenderFillMode::Solid;
        resource.Binding.HasFillModeOverride = false;
        return resource;
    }
} // namespace Wayfinder