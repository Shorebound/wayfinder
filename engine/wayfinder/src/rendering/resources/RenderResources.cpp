#include "RenderResources.h"

#include "rendering/materials/Material.h"
#include "rendering/resources/TextureManager.h"

#include <filesystem>
#include <unordered_set>

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

    void RenderResourceCache::SetTextureManager(TextureManager* textureManager)
    {
        m_textureManager = textureManager;
        m_materialsByKey.clear();
    }

    void RenderResourceCache::SetProgramRegistry(const ShaderProgramRegistry* programs)
    {
        if (m_programs == programs)
        {
            return;
        }

        m_programs = programs;
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

            if (!pass.DebugDraw.has_value())
            {
                continue;
            }

            RenderDebugDrawList debugDraw = pass.DebugDraw.value_or(RenderDebugDrawList{});
            for (RenderDebugBox& debugBox : debugDraw.Boxes)
            {
                debugBox.Material = PrepareMaterialBinding(debugBox.Material);
            }
            pass.DebugDraw = std::move(debugDraw);
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
        const auto [it, inserted] = m_meshesByKey.emplace(submission.Mesh.StableKey, resource);
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

        // Replace texture bindings with the material asset's authored textures
        resource.Binding.Textures.Slots = materialAsset->Textures;

        // Resolve texture bindings to GPU handles once at cache time
        ResolveTextureBindings(resource.Binding);

        return resource;
    }

    void RenderResourceCache::ResolveTextureBindings(RenderMaterialBinding& binding)
    {
        binding.ResolvedTextures.clear();

        if (!m_textureManager || !m_programs)
        {
            return;
        }

        const ShaderProgram* program = m_programs->FindOrDefault(binding.ShaderName);
        if (!program)
        {
            return;
        }

        // Track which authored texture keys are consumed by the shader
        std::unordered_set<std::string> consumedSlots;

        for (const auto& slotDecl : program->Desc.TextureSlots)
        {
            ResolvedTextureBinding resolved;
            resolved.Slot = slotDecl.BindingSlot;

            // Look up the texture asset ID for this slot
            auto slotIt = binding.Textures.Slots.find(slotDecl.Name);
            if (slotIt != binding.Textures.Slots.end() && m_assetService)
            {
                consumedSlots.insert(slotDecl.Name);
                resolved.Texture = m_textureManager->GetOrLoad(slotIt->second, *m_assetService);
            }
            else
            {
                resolved.Texture = m_textureManager->GetFallback();
            }

            // Create/get a sampler for this texture asset's filter/address settings
            std::string texError;
            const TextureAsset* texAsset = (slotIt != binding.Textures.Slots.end() && m_assetService) ? m_assetService->LoadAsset<TextureAsset>(slotIt->second, texError) : nullptr;

            SamplerCreateDesc samplerDesc;
            if (texAsset)
            {
                samplerDesc.minFilter = texAsset->Filter;
                samplerDesc.magFilter = texAsset->Filter;
                samplerDesc.addressModeU = texAsset->AddressMode;
                samplerDesc.addressModeV = texAsset->AddressMode;

                // Enable trilinear filtering when the texture has mipmaps
                const bool hasMips = (texAsset->MipLevels != 1);
                if (hasMips && texAsset->Filter == SamplerFilter::Linear)
                {
                    samplerDesc.mipmapMode = SamplerMipmapMode::Linear;
                    samplerDesc.enableAnisotropy = true;
                    samplerDesc.maxAnisotropy = 4.0f;
                }
            }
            resolved.Sampler = m_textureManager->GetOrCreateSampler(samplerDesc);

            binding.ResolvedTextures.push_back(resolved);
        }

        // Warn about authored texture keys that don't match any shader slot
        for (const auto& [slotName, assetId] : binding.Textures.Slots)
        {
            if (!consumedSlots.contains(slotName))
            {
                WAYFINDER_WARNING(
                    LogRenderer, "RenderResourceCache: Material '{}' shader '{}' has no slot '{}' — texture ignored", binding.Ref.AssetId ? binding.Ref.AssetId->ToString() : "<unknown>", binding.ShaderName, slotName);
            }
        }
    }
} // namespace Wayfinder