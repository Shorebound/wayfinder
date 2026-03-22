#include "assets/TextureAsset.h"
#include "assets/AssetCache.h"
#include "assets/AssetRegistry.h"
#include "rendering/materials/Material.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

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
        CHECK(asset.PixelData.capacity() == 0);
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
        AssetCache<MaterialAsset> cache;
        CHECK(cache.Size() == 0);
    }

    TEST_CASE("AssetCache Get returns nullptr on miss")
    {
        AssetCache<MaterialAsset> cache;
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
        AssetCache<MaterialAsset> cache;
        cache.Clear();
        CHECK(cache.Size() == 0);
    }

    TEST_CASE("AssetCache LoadOrGet loads from registry and caches")
    {
        /// Set up a temp directory with a single material asset JSON.
        const auto tempDir = Helpers::FixturesDir() / "temp" / "load_or_get_test";
        std::filesystem::create_directories(tempDir);

        const std::string assetIdText = "f0000000-0000-0000-0000-000000000001";
        const auto assetId = AssetId::Parse(assetIdText).value();

        {
            std::ofstream file(tempDir / "test_material.json");
            file << R"({
                "asset_id": ")" + assetIdText + R"(",
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

} // namespace Wayfinder::Tests
