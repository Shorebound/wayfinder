#include "Material.h"

namespace
{
    constexpr std::string_view kAssetIdKey = "asset_id";
    constexpr std::string_view kAssetTypeKey = "asset_type";
    constexpr std::string_view kNameKey = "name";
    constexpr std::string_view kShaderKey = "shader";
    constexpr std::string_view kBaseColorKey = "base_color";
    constexpr std::string_view kParametersKey = "parameters";

    // Parse a TOML array of 3 or 4 integers into a LinearColor.
    bool ParseLinearColor(const toml::array* values, Wayfinder::LinearColor& colour, std::string& error)
    {
        if (!values || (values->size() != 3 && values->size() != 4))
        {
            error = "must be an array of 3 or 4 integers";
            return false;
        }

        const auto readChannel = [&](size_t index, float fallback) -> float
        {
            return static_cast<float>(values->get(index)->value_or(static_cast<int64_t>(static_cast<uint8_t>(fallback * 255.0f)))) / 255.0f;
        };

        colour.r = readChannel(0,colour.r);
        colour.g = readChannel(1, colour.g);
        colour.b = readChannel(2, colour.b);
        colour.a = values->size() == 4 ? readChannel(3, colour.a) : colour.a;
        return true;
    }

    // Parse a TOML [parameters] table into a MaterialParameterBlock.
    // Supports: arrays of 3–4 numbers as Color, single numbers as Float, integers as Int.
    void ParseParametersTable(const toml::table& params, Wayfinder::MaterialParameterBlock& block)
    {
        for (const auto& [key, node] : params)
        {
            const std::string name{key.str()};

            if (node.is_array())
            {
                const auto* arr = node.as_array();
                if (arr->size() >= 3 && arr->size() <= 4)
                {
                    // Treat as Color (integer RGBA → LinearColor)
                    Wayfinder::LinearColor colour = Wayfinder::LinearColor::White();
                    std::string unused;
                    if (ParseLinearColor(arr, colour, unused))
                    {
                        block.SetColor(name, colour);
                    }
                }
                else if (arr->size() == 2)
                {
                    glm::vec2 v{
                        static_cast<float>(arr->get(0)->value_or(0.0)),
                        static_cast<float>(arr->get(1)->value_or(0.0))};
                    block.SetVec2(name, v);
                }
            }
            else if (node.is_floating_point())
            {
                block.SetFloat(name, static_cast<float>(node.value_or(0.0)));
            }
            else if (node.is_integer())
            {
                block.SetInt(name, static_cast<int32_t>(node.value_or(static_cast<int64_t>(0))));
            }
        }
    }
}

namespace Wayfinder
{
    LinearColor MaterialAsset::GetBaseColor() const
    {
        auto it = Parameters.Values.find("base_color");
        if (it != Parameters.Values.end())
        {
            if (const auto* c = std::get_if<LinearColor>(&it->second)) return *c;
        }
        return LinearColor::White();
    }

    void MaterialAsset::SetBaseColor(const LinearColor& colour)
    {
        Parameters.SetColor("base_color", colour);
    }

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
        parsed.ShaderName = document[kShaderKey].value_or(std::string("unlit"));

        // Parse [parameters] table if present (new format)
        if (const auto* paramsTable = document.get_as<toml::table>(kParametersKey))
        {
            ParseParametersTable(*paramsTable, parsed.Parameters);
        }

        // Legacy support: top-level base_color → parameters["base_color"]
        // Only applied if [parameters] didn't already set it.
        if (!parsed.Parameters.Has("base_color") && document.contains(kBaseColorKey))
        {
            const toml::array* values = document.get_as<toml::array>(kBaseColorKey);
            LinearColor baseColor = LinearColor::White();
            std::string colorError;
            if (!ParseLinearColor(values, baseColor, colorError))
            {
                error = "Material asset '" + sourceLabel + "' field 'base_color' " + colorError;
                return false;
            }
            parsed.Parameters.SetColor("base_color", baseColor);
        }

        // Default base_color to white if nothing was specified
        if (!parsed.Parameters.Has("base_color"))
        {
            parsed.Parameters.SetColor("base_color", LinearColor::White());
        }

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
        toml::table table;
        table.insert_or_assign("material_id", material.Id.ToString());
        table.insert_or_assign("shader", material.ShaderName);

        // Serialise parameters as a [parameters] table
        toml::table paramsTable;
        for (const auto& [name, value] : material.Parameters.Values)
        {
            std::visit([&paramsTable, &name](auto&& v)
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, LinearColor>)
                {
                    toml::array arr;
                    arr.push_back(static_cast<int64_t>(v.r * 255.0f));
                    arr.push_back(static_cast<int64_t>(v.g * 255.0f));
                    arr.push_back(static_cast<int64_t>(v.b * 255.0f));
                    arr.push_back(static_cast<int64_t>(v.a * 255.0f));
                    paramsTable.insert_or_assign(name, std::move(arr));
                }
                else if constexpr (std::is_same_v<T, float>)
                {
                    paramsTable.insert_or_assign(name, static_cast<double>(v));
                }
                else if constexpr (std::is_same_v<T, int32_t>)
                {
                    paramsTable.insert_or_assign(name, static_cast<int64_t>(v));
                }
                else if constexpr (std::is_same_v<T, glm::vec2>)
                {
                    toml::array arr;
                    arr.push_back(static_cast<double>(v.x));
                    arr.push_back(static_cast<double>(v.y));
                    paramsTable.insert_or_assign(name, std::move(arr));
                }
                else if constexpr (std::is_same_v<T, glm::vec3>)
                {
                    toml::array arr;
                    arr.push_back(static_cast<double>(v.x));
                    arr.push_back(static_cast<double>(v.y));
                    arr.push_back(static_cast<double>(v.z));
                    paramsTable.insert_or_assign(name, std::move(arr));
                }
                else if constexpr (std::is_same_v<T, glm::vec4>)
                {
                    toml::array arr;
                    arr.push_back(static_cast<double>(v.x));
                    arr.push_back(static_cast<double>(v.y));
                    arr.push_back(static_cast<double>(v.z));
                    arr.push_back(static_cast<double>(v.w));
                    paramsTable.insert_or_assign(name, std::move(arr));
                }
            }, value);
        }

        if (!paramsTable.empty())
        {
            table.insert_or_assign("parameters", std::move(paramsTable));
        }

        // Also write top-level base_color for backward compatibility
        LinearColor baseColor = material.GetBaseColor();
        toml::array baseColorArr;
        baseColorArr.push_back(static_cast<int64_t>(baseColor.r * 255.0f));
        baseColorArr.push_back(static_cast<int64_t>(baseColor.g * 255.0f));
        baseColorArr.push_back(static_cast<int64_t>(baseColor.b * 255.0f));
        baseColorArr.push_back(static_cast<int64_t>(baseColor.a * 255.0f));
        table.insert_or_assign("base_color", std::move(baseColorArr));

        return table;
    }
} // namespace Wayfinder
