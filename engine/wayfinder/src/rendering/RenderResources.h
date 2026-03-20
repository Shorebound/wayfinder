#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>

#include "../assets/AssetService.h"
#include "Material.h"
#include "RenderFrame.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    struct RenderMeshResource
    {
        RenderMeshRef Ref{};
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
        void SetAssetService(const std::shared_ptr<AssetService>& assetService);
        void PrepareFrame(RenderFrame& frame);

        const RenderMeshResource& ResolveMesh(const RenderMeshSubmission& submission);

    private:
        RenderMaterialBinding PrepareMaterialBinding(const RenderMaterialBinding& binding);
        RenderMaterialResource CreateMaterialResource(const RenderMaterialBinding& binding);

        std::shared_ptr<AssetService> m_assetService;
        std::unordered_map<uint64_t, RenderMeshResource> m_meshesByKey;
        std::unordered_map<uint64_t, RenderMaterialResource> m_materialsByKey;
    };
} // namespace Wayfinder