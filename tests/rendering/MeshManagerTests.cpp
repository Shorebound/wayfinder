#include "assets/AssetService.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/resources/MeshManager.h"

#include <doctest/doctest.h>

namespace Wayfinder::Tests
{
    TEST_CASE("MeshManager Shutdown is safe on uninitialised manager")
    {
        MeshManager manager;
        CHECK_NOTHROW(manager.Shutdown());
    }

    TEST_CASE("MeshManager GetOrLoad without Initialise returns fallback reference")
    {
        MeshManager manager;
        AssetService assets;
        std::string err;
        REQUIRE(assets.SetAssetRoot(std::filesystem::path{}, err));

        const AssetId id = AssetId::Generate();
        Mesh* m = manager.GetOrLoad(id, assets);
        REQUIRE(m);
        CHECK(&manager.GetFallbackMesh() == m);
    }

} // namespace Wayfinder::Tests
