#include "Material.h"

namespace
{
    constexpr std::string_view kAssetIdKey = "asset_id";
    constexpr std::string_view kAssetTypeKey = "asset_type";
    constexpr std::string_view kNameKey = "name";
    constexpr std::string_view kBaseColorKey = "base_color";
    constexpr std::string_view kWireframeKey = "wireframe";

    bool ParseColor(const toml::table& table, std::string_view key, Wayfinder::Color& color, std::string& error)
    {
        const toml::array* values = table.get_as<toml::array>(key);
        if (!values || (values->size() != 3 && values->size() != 4))
        {
            error = "field '" + std::string{key} + "' must be an array of 3 or 4 integers";
            return false;
        }

        const auto readChannel = [&](size_t index, uint8_t fallback) -> uint8_t
        {
            return static_cast<uint8_t>(values->get(index)->value_or(static_cast<int64_t>(fallback)));
        };

        color.r = readChannel(0, color.r);
        color.g = readChannel(1, color.g);
        color.b = readChannel(2, color.b);
        color.a = values->size() == 4 ? readChannel(3, color.a) : color.a;
        return true;
    }
}

namespace Wayfinder
{
    bool ParseMaterialAssetDocument(
        const toml::table& document,
        const std::string& sourceLabel,
        MaterialAsset& material,
        std::string& error)
    {
        const auto assetIdText = document[kAssetIdKey].value<std::string>();
        if (!assetIdText)
        {
            error = "Material asset '" + sourceLabel + "' is missing asset_id";
            return false;
        }

        const std::optional<AssetId> assetId = AssetId::Parse(*assetIdText);
        if (!assetId)
        {
            error = "Material asset '" + sourceLabel + "' has an invalid asset_id";
            return false;
        }

        const auto assetType = document[kAssetTypeKey].value<std::string>();
        if (!assetType)
        {
            error = "Material asset '" + sourceLabel + "' is missing asset_type";
            return false;
        }

        if (*assetType != "material")
        {
            error = "Material asset '" + sourceLabel + "' must declare asset_type = 'material'";
            return false;
        }

        MaterialAsset parsed;
        parsed.Id = *assetId;
        parsed.Name = document[kNameKey].value_or(std::filesystem::path(sourceLabel).stem().string());

        if (document.contains(kBaseColorKey) && !ParseColor(document, kBaseColorKey, parsed.BaseColor, error))
        {
            error = "Material asset '" + sourceLabel + "' " + error;
            return false;
        }

        const toml::node* wireframeNode = document.get(kWireframeKey);
        if (wireframeNode && !wireframeNode->is_boolean())
        {
            error = "Material asset '" + sourceLabel + "' field 'wireframe' must be a boolean";
            return false;
        }

        parsed.Wireframe = document[kWireframeKey].value_or(parsed.Wireframe);
        material = std::move(parsed);
        return true;
    }

    bool LoadMaterialAssetFromFile(
        const std::filesystem::path& filePath,
        MaterialAsset& material,
        std::string& error)
    {
        try
        {
            const toml::table document = toml::parse_file(filePath.string());
            return ParseMaterialAssetDocument(document, filePath.generic_string(), material, error);
        }
        catch (const toml::parse_error& parseError)
        {
            error = "Failed to parse material asset '" + filePath.generic_string() + "': " + std::string{parseError.description()};
            return false;
        }
    }

    toml::table CreateMaterialComponentTable(const MaterialAsset& material)
    {
        toml::array baseColor;
        baseColor.push_back(static_cast<int64_t>(material.BaseColor.r));
        baseColor.push_back(static_cast<int64_t>(material.BaseColor.g));
        baseColor.push_back(static_cast<int64_t>(material.BaseColor.b));
        baseColor.push_back(static_cast<int64_t>(material.BaseColor.a));

        toml::table table;
        table.insert_or_assign("material_id", material.Id.ToString());
        table.insert_or_assign("base_color", std::move(baseColor));
        table.insert_or_assign("wireframe", material.Wireframe);
        return table;
    }
} // namespace Wayfinder
