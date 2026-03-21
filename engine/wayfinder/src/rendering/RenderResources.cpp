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
        resource.Ref = submission.Mesh;
        resource.Geometry = submission.Geometry;
        const auto [it, inserted] = m_meshesByKey.emplace(submission.Mesh.StableKey, std::move(resource));
        return it->second;
    }

    RenderMaterialBinding RenderResourceCache::PrepareMaterialBinding(const RenderMaterialBinding& binding)
    {
        if (binding.Ref.Origin != RenderResourceOrigin::Asset)
        {
            return binding;
        }

        auto existing = m_materialsByKey.find(binding.Ref.StableKey);
        if (existing == m_materialsByKey.end())
        {
            existing = m_materialsByKey.emplace(binding.Ref.StableKey, CreateMaterialResource(binding)).first;
        }

        RenderMaterialBinding resolved = existing->second.Binding;
        resolved.Ref = binding.Ref;
        resolved.Domain = binding.Domain;

        // Apply per-entity overrides on top of asset-loaded parameters
        if (binding.HasOverrides)
        {
            resolved.HasOverrides = true;
            resolved.Overrides = binding.Overrides;
        }

        if (binding.StateOverrides.FillMode)
        {
            resolved.StateOverrides.FillMode = binding.StateOverrides.FillMode;
        }

        return resolved;
    }

    RenderMaterialResource RenderResourceCache::CreateMaterialResource(const RenderMaterialBinding& binding)
    {
        RenderMaterialResource resource;
        resource.Ref = binding.Ref;
        resource.Binding = binding;

        if (binding.Ref.Origin != RenderResourceOrigin::Asset || !binding.Ref.AssetId || !m_assetService)
        {
            return resource;
        }

        std::string error;
        const MaterialAsset* materialAsset = m_assetService->LoadMaterialAsset(*binding.Ref.AssetId, error);
        if (!materialAsset)
        {
            return resource;
        }

        resource.IsLoadedFromAsset = true;
        resource.Binding.ShaderName = materialAsset->ShaderName;
        resource.Binding.Parameters = materialAsset->Parameters;
        resource.Binding.HasOverrides = false;
        return resource;
    }
} // namespace Wayfinder