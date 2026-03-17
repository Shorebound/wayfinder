#include "RenderResources.h"

#include "../graphics/Material.h"

#include <filesystem>

namespace Wayfinder
{
    void RenderResourceCache::SetAssetRoot(const std::filesystem::path& assetRoot)
    {
        const std::filesystem::path normalizedRoot = assetRoot.empty() ? std::filesystem::path{} : std::filesystem::weakly_canonical(assetRoot);
        if (normalizedRoot == m_assetRoot)
        {
            return;
        }

        m_assetRoot = normalizedRoot;
        m_hasAssetRegistry = false;
        m_materialAssetsById.clear();
        m_missingMaterialAssets.clear();
        m_materialsByKey.clear();

        if (!m_assetRoot.empty())
        {
            std::string error;
            m_hasAssetRegistry = m_assetRegistry.BuildFromDirectory(m_assetRoot, error);
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

    RenderMaterialBinding RenderResourceCache::ResolveMaterialBinding(const RenderMaterialBinding& binding)
    {
        auto existing = m_materialsByKey.find(binding.Handle.StableKey);
        if (existing == m_materialsByKey.end())
        {
            existing = m_materialsByKey.emplace(binding.Handle.StableKey, CreateMaterialResource(binding)).first;
        }

        RenderMaterialBinding resolved = existing->second.Binding;
        resolved.Handle = binding.Handle;
        resolved.Domain = binding.Domain;

        if (binding.Handle.Origin != RenderResourceOrigin::Asset)
        {
            resolved = binding;
            return resolved;
        }

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

        if (binding.Handle.Origin != RenderResourceOrigin::Asset || !binding.Handle.AssetId || m_assetRoot.empty() || !m_hasAssetRegistry)
        {
            return resource;
        }

        if (m_missingMaterialAssets.contains(*binding.Handle.AssetId))
        {
            return resource;
        }

        if (const auto cached = m_materialAssetsById.find(*binding.Handle.AssetId); cached != m_materialAssetsById.end())
        {
            resource.IsLoadedFromAsset = true;
            resource.Binding.BaseColor = cached->second.BaseColor;
            resource.Binding.HasBaseColorOverride = false;
            resource.Binding.FillMode = cached->second.Wireframe ? RenderFillMode::SolidAndWireframe : RenderFillMode::Solid;
            resource.Binding.HasFillModeOverride = false;
            return resource;
        }

        const std::filesystem::path* materialPath = m_assetRegistry.ResolvePath(*binding.Handle.AssetId);
        if (!materialPath)
        {
            m_missingMaterialAssets.emplace(*binding.Handle.AssetId, true);
            return resource;
        }

        std::string error;
        MaterialAsset materialAsset;
        if (!LoadMaterialAssetFromFile(*materialPath, materialAsset, error))
        {
            m_missingMaterialAssets.emplace(*binding.Handle.AssetId, true);
            return resource;
        }

        m_materialAssetsById.emplace(materialAsset.Id, materialAsset);

        resource.IsLoadedFromAsset = true;
        resource.Binding.BaseColor = materialAsset.BaseColor;
        resource.Binding.HasBaseColorOverride = false;
        resource.Binding.WireframeColor = binding.WireframeColor;
        resource.Binding.FillMode = materialAsset.Wireframe ? RenderFillMode::SolidAndWireframe : RenderFillMode::Solid;
        resource.Binding.HasFillModeOverride = false;
        return resource;
    }
} // namespace Wayfinder