#include "assets/AssetCache.h"
#include "assets/AssetRegistry.h"
#include "assets/MeshAsset.h"
#include "assets/MeshFormat.h"
#include "assets/TextureAsset.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/materials/Material.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "TestHelpers.h"

namespace Wayfinder::Tests
{
    // ── TextureAsset Validation ──────────────────────────────

    TEST_CASE("ValidateTextureAssetDocument accepts valid document")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_id": "a0000000-0000-0000-0000-000000000001",
            "asset_type": "texture",
            "name": "test",
            "source": "test.png"
        })");

        std::string error;
        CHECK(ValidateTextureAssetDocument(doc, "test.json", error));
        CHECK(error.empty());
    }

    TEST_CASE("ValidateTextureAssetDocument rejects missing asset_id")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_type": "texture",
            "source": "test.png"
        })");

        std::string error;
        CHECK_FALSE(ValidateTextureAssetDocument(doc, "test.json", error));
        CHECK(error.find("asset_id") != std::string::npos);
    }

    TEST_CASE("ValidateTextureAssetDocument rejects missing asset_type")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_id": "a0000000-0000-0000-0000-000000000001",
            "source": "test.png"
        })");

        std::string error;
        CHECK_FALSE(ValidateTextureAssetDocument(doc, "test.json", error));
        CHECK(error.find("asset_type") != std::string::npos);
    }

    TEST_CASE("ValidateTextureAssetDocument rejects wrong asset_type")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_id": "a0000000-0000-0000-0000-000000000001",
            "asset_type": "material",
            "source": "test.png"
        })");

        std::string error;
        CHECK_FALSE(ValidateTextureAssetDocument(doc, "test.json", error));
        CHECK(error.find("texture") != std::string::npos);
    }

    TEST_CASE("ValidateTextureAssetDocument rejects missing source")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_id": "a0000000-0000-0000-0000-000000000001",
            "asset_type": "texture"
        })");

        std::string error;
        CHECK_FALSE(ValidateTextureAssetDocument(doc, "test.json", error));
        CHECK(error.find("source") != std::string::npos);
    }

    TEST_CASE("ValidateTextureAssetDocument accepts optional filter and address_mode")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_id": "a0000000-0000-0000-0000-000000000001",
            "asset_type": "texture",
            "source": "test.png",
            "filter": "nearest",
            "address_mode": "clamp"
        })");

        std::string error;
        CHECK(ValidateTextureAssetDocument(doc, "test.json", error));
    }

    TEST_CASE("ValidateTextureAssetDocument rejects non-string filter")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_id": "a0000000-0000-0000-0000-000000000001",
            "asset_type": "texture",
            "source": "test.png",
            "filter": 42
        })");

        std::string error;
        CHECK_FALSE(ValidateTextureAssetDocument(doc, "test.json", error));
        CHECK(error.find("filter") != std::string::npos);
    }

    TEST_CASE("ValidateTextureAssetDocument rejects non-string address_mode")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_id": "a0000000-0000-0000-0000-000000000001",
            "asset_type": "texture",
            "source": "test.png",
            "address_mode": true
        })");

        std::string error;
        CHECK_FALSE(ValidateTextureAssetDocument(doc, "test.json", error));
        CHECK(error.find("address_mode") != std::string::npos);
    }

    // ── TextureAsset ReleasePixelData ────────────────────────

    TEST_CASE("ReleasePixelData clears and shrinks pixel data")
    {
        TextureAsset asset;
        asset.PixelData.resize(1024, 0xFF);
        CHECK(asset.PixelData.size() == 1024);

        asset.ReleasePixelData();
        CHECK(asset.PixelData.empty());
    }

    // ── Material Texture Parsing ─────────────────────────────

    TEST_CASE("Material parsing loads texture slots")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_id": "a0000000-0000-0000-0000-000000000001",
            "asset_type": "material",
            "shader": "textured_lit",
            "textures": {
                "diffuse": "b0000000-0000-0000-0000-000000000001",
                "normal": "c0000000-0000-0000-0000-000000000002"
            }
        })");

        MaterialAsset material;
        std::string error;
        CHECK(ParseMaterialAssetDocument(doc, "test_material.json", material, error));
        CHECK(material.Textures.size() == 2);
        CHECK(material.Textures.contains("diffuse"));
        CHECK(material.Textures.contains("normal"));
        CHECK(material.Textures.at("diffuse").ToString() == "b0000000-0000-0000-0000-000000000001");
    }

    TEST_CASE("Material parsing rejects invalid texture asset ID")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_id": "a0000000-0000-0000-0000-000000000001",
            "asset_type": "material",
            "shader": "textured_lit",
            "textures": {
                "diffuse": "not-a-uuid"
            }
        })");

        MaterialAsset material;
        std::string error;
        CHECK_FALSE(ParseMaterialAssetDocument(doc, "test_material.json", material, error));
        CHECK(error.find("invalid asset ID") != std::string::npos);
    }

    TEST_CASE("Material parsing accepts empty textures block")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_id": "a0000000-0000-0000-0000-000000000001",
            "asset_type": "material",
            "shader": "basic_lit",
            "textures": {}
        })");

        MaterialAsset material;
        std::string error;
        CHECK(ParseMaterialAssetDocument(doc, "test_material.json", material, error));
        CHECK(material.Textures.empty());
    }

    TEST_CASE("Material parsing works without textures block")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_id": "a0000000-0000-0000-0000-000000000001",
            "asset_type": "material",
            "shader": "basic_lit"
        })");

        MaterialAsset material;
        std::string error;
        CHECK(ParseMaterialAssetDocument(doc, "test_material.json", material, error));
        CHECK(material.Textures.empty());
    }

    // ── AssetCache ───────────────────────────────────────────

    TEST_CASE("AssetCache starts empty")
    {
        const AssetCache<MaterialAsset> cache;
        CHECK(cache.Size() == 0);
    }

    TEST_CASE("AssetCache Get returns nullptr on miss")
    {
        const AssetCache<MaterialAsset> cache;
        const AssetId id = AssetId::Generate();
        CHECK(cache.Get(id) == nullptr);
    }

    TEST_CASE("AssetCache GetMutable returns nullptr on miss")
    {
        AssetCache<MaterialAsset> cache;
        const AssetId id = AssetId::Generate();
        CHECK(cache.GetMutable(id) == nullptr);
    }

    TEST_CASE("AssetCache Clear resets size")
    {
        const auto tempDir = Helpers::FixturesDir() / "temp" / "clear_test";
        std::filesystem::create_directories(tempDir);

        const std::string assetIdText = "f0000000-0000-0000-0000-000000000099";
        const auto parsedAssetId = AssetId::Parse(assetIdText);
        REQUIRE(parsedAssetId.has_value());
        const AssetId assetId = *parsedAssetId;

        {
            std::ofstream file(tempDir / "clear_material.json");
            file << R"({
                "asset_id": ")" +
                        assetIdText + R"(",
                "asset_type": "material",
                "name": "clear_test",
                "shader": "basic_lit"
            })";
        }

        AssetRegistry registry;
        std::string error;
        REQUIRE(registry.BuildFromDirectory(tempDir, error));

        AssetCache<MaterialAsset> cache;
        REQUIRE(cache.LoadOrGet(assetId, registry, error) != nullptr);
        CHECK(cache.Size() == 1);

        cache.Clear();
        CHECK(cache.Size() == 0);
        CHECK(cache.Get(assetId) == nullptr);

        std::filesystem::remove_all(tempDir);
    }

    TEST_CASE("AssetCache LoadOrGet loads from registry and caches")
    {
        /// Set up a temp directory with a single material asset JSON.
        const auto tempDir = Helpers::FixturesDir() / "temp" / "load_or_get_test";
        std::filesystem::create_directories(tempDir);

        const std::string assetIdText = "f0000000-0000-0000-0000-000000000001";
        const auto parsedAssetId = AssetId::Parse(assetIdText);
        REQUIRE(parsedAssetId.has_value());
        const AssetId assetId = *parsedAssetId;

        {
            std::ofstream file(tempDir / "test_material.json");
            file << R"({
                "asset_id": ")" +
                        assetIdText + R"(",
                "asset_type": "material",
                "name": "load_or_get_test",
                "shader": "basic_lit"
            })";
        }

        AssetRegistry registry;
        std::string error;
        REQUIRE(registry.BuildFromDirectory(tempDir, error));

        AssetCache<MaterialAsset> cache;
        CHECK(cache.Size() == 0);

        // First call — cache miss, loads from disk
        const MaterialAsset* first = cache.LoadOrGet(assetId, registry, error);
        REQUIRE(first != nullptr);
        CHECK(cache.Size() == 1);
        CHECK(cache.Get(assetId) != nullptr);

        // Second call — cache hit, returns same pointer
        const MaterialAsset* second = cache.LoadOrGet(assetId, registry, error);
        CHECK(second == first);
        CHECK(cache.Size() == 1);

        // Clean up temp files
        std::filesystem::remove_all(tempDir);
    }

    TEST_CASE("ValidateMeshAssetDocument accepts valid mesh descriptor")
    {
        const auto doc = nlohmann::json::parse(R"({
            "asset_id": "a0000000-0000-0000-0000-000000000050",
            "asset_type": "mesh",
            "name": "test_mesh",
            "source": "x.wfmesh",
            "submeshes": [ { "name": "a", "material_slot": 0 } ]
        })");

        std::string error;
        CHECK(ValidateMeshAssetDocument(doc, "mesh.json", error));
        CHECK(error.empty());
    }

    TEST_CASE("LoadMeshAssetFromDocument reads paired binary")
    {
        const auto tempDir = Helpers::FixturesDir() / "temp" / "mesh_asset_load";
        std::filesystem::create_directories(tempDir);

        ParsedMeshFile parsed{};
        parsed.Header.Magic = MESH_FILE_MAGIC;
        parsed.Header.Version = MESH_FILE_VERSION;
        parsed.Header.SubmeshCount = 1;
        parsed.Header.Bounds = AxisAlignedBounds{.Min = Float3{0, 0, 0}, .Max = Float3{1, 1, 0}};

        SubmeshCpuData sm{};
        sm.VertexFormat = MeshVertexFormat::PosNormalUVTangent;
        sm.IndexFormat = MeshIndexFormat::Uint16;
        sm.VertexCount = 3;
        sm.IndexCount = 3;
        sm.MaterialSlot = 0;
        sm.Bounds = parsed.Header.Bounds;

        const std::array<VertexPositionNormalUVTangent, 3> verts{};
        sm.VertexBytes.resize(sizeof(verts));
        std::memcpy(sm.VertexBytes.data(), verts.data(), sm.VertexBytes.size());

        const std::array<std::uint16_t, 3> idx{0, 1, 2};
        sm.IndexBytes.resize(sizeof(idx));
        std::memcpy(sm.IndexBytes.data(), idx.data(), sm.IndexBytes.size());

        parsed.Submeshes.push_back(std::move(sm));
        parsed.SubmeshTable.assign(1, SubmeshTableEntry{});

        std::vector<std::byte> bin;
        std::string werr;
        REQUIRE(WriteMeshFileV1(parsed, bin, werr));

        {
            std::ofstream bf(tempDir / "unit.wfmesh", std::ios::binary);
            bf.write(reinterpret_cast<const char*>(bin.data()), static_cast<std::streamsize>(bin.size()));
        }

        const std::string assetIdText = "a0000000-0000-0000-0000-000000000051";
        {
            const std::string json = std::string(R"({
    "asset_id": ")") + assetIdText + R"(",
    "asset_type": "mesh",
    "name": "unit",
    "source": "unit.wfmesh"
})";
            std::ofstream jf(tempDir / "unit_mesh.json");
            jf << json;
        }

        nlohmann::json doc;
        {
            std::ifstream jf(tempDir / "unit_mesh.json");
            jf >> doc;
        }
        std::string error;
        const auto loaded = LoadMeshAssetFromDocument(doc, tempDir / "unit_mesh.json", error);
        REQUIRE(loaded.has_value());
        const MeshAsset& loadedAsset = *loaded;
        CHECK(loadedAsset.Submeshes.size() == 1);
        CHECK(loadedAsset.Submeshes.at(0).VertexCount == 3u);

        std::filesystem::remove_all(tempDir);
    }

} // namespace Wayfinder::Tests
