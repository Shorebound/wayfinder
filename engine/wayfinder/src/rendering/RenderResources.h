#pragma once

#include <filesystem>
#include <optional>
#include <unordered_map>

#include "../assets/AssetRegistry.h"
#include "../graphics/Material.h"
#include "RenderFrame.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    struct RenderMeshResource
    {
        RenderMeshHandle Handle{};
        RenderGeometry Geometry{};
    };

    struct RenderMaterialResource
    {
        RenderMaterialHandle Handle{};
        RenderMaterialBinding Binding{};
        bool IsLoadedFromAsset = false;
    };

    class WAYFINDER_API RenderResourceCache
    {
    public:
        void SetAssetRoot(const std::filesystem::path& assetRoot);

        const RenderMeshResource& ResolveMesh(const RenderMeshSubmission& submission);
        RenderMaterialBinding ResolveMaterialBinding(const RenderMaterialBinding& binding);

    private:
        RenderMaterialResource CreateMaterialResource(const RenderMaterialBinding& binding);

        std::filesystem::path m_assetRoot;
        AssetRegistry m_assetRegistry;
        bool m_hasAssetRegistry = false;
        std::unordered_map<uint64_t, RenderMeshResource> m_meshesByKey;
        std::unordered_map<uint64_t, RenderMaterialResource> m_materialsByKey;
        std::unordered_map<AssetId, MaterialAsset> m_materialAssetsById;
        std::unordered_map<AssetId, bool> m_missingMaterialAssets;
    };
} // namespace Wayfinder