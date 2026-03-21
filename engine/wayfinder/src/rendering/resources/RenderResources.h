#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>

#include "assets/AssetService.h"
#include "rendering/materials/Material.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/graph/RenderFrame.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class TextureManager;
    struct RenderMeshResource
    {
        RenderMeshRef Ref{};
        RenderGeometry Geometry{};
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
        void SetTextureManager(TextureManager* textureManager);
        void SetProgramRegistry(const ShaderProgramRegistry* programs);
        void PrepareFrame(RenderFrame& frame);

        const RenderMeshResource& ResolveMesh(const RenderMeshSubmission& submission);

    private:
        RenderMaterialBinding PrepareMaterialBinding(const RenderMaterialBinding& binding);
        RenderMaterialResource CreateMaterialResource(const RenderMaterialBinding& binding);
        void ResolveTextureBindings(RenderMaterialBinding& binding);

        std::shared_ptr<AssetService> m_assetService;
        TextureManager* m_textureManager = nullptr;
        const ShaderProgramRegistry* m_programs = nullptr;
        std::unordered_map<uint64_t, RenderMeshResource> m_meshesByKey;
        std::unordered_map<uint64_t, RenderMaterialResource> m_materialsByKey;
    };
} // namespace Wayfinder