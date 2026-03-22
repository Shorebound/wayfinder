#include "Material.h"
#include "core/Types.h"
#include <fstream>

namespace Wayfinder
{
    constexpr std::string_view kAssetIdKey = "asset_id";
    constexpr std::string_view kAssetTypeKey = "asset_type";
    constexpr std::string_view kNameKey = "name";
    constexpr std::string_view kShaderKey = "shader";
    constexpr std::string_view kBaseColourKey = "base_colour";
    constexpr std::string_view kWireframeKey = "wireframe";
    constexpr std::string_view kParametersKey = "parameters";
    constexpr std::string_view kTexturesKey = "textures";

    /// Parse a JSON array of 3 or 4 integers into a LinearColour.
    /// Returns false if any channel is not an integer.
    bool ParseLinearColour(const nlohmann::json& values, Wayfinder::LinearColour& colour, std::string& error)
    {
        if (!values.is_array() || (values.size() != 3 && values.size() != 4))
        {
            error = "must be an array of 3 or 4 integers";
            return false;
        }

        for (size_t i = 0; i < values.size(); ++i)
        {
            if (!values[i].is_number_integer())
            {
                error = "must be an array of 3 or 4 integers";
                return false;
            }
        }

        colour.r = static_cast<float>(values[0].get<int64_t>()) / 255.0f;
        colour.g = static_cast<float>(values[1].get<int64_t>()) / 255.0f;
        colour.b = static_cast<float>(values[2].get<int64_t>()) / 255.0f;
        colour.a = values.size() == 4 ? static_cast<float>(values[3].get<int64_t>()) / 255.0f : colour.a;
        return true;
    }

    /// Parse a JSON "parameters" object into a MaterialParameterBlock.
    /// Supports: arrays of 3–4 numbers as Colour, single numbers as Float, integers as Int.
    void ParseParametersTable(const nlohmann::json& params, Wayfinder::MaterialParameterBlock& block)
    {
        if (!params.is_object())
        {
            return;
        }

        for (const auto& [key, node] : params.items())
        {
            const std::string name{key};

            if (node.is_array())
            {
                if (node.size() >= 3 && node.size() <= 4)
                {
                    // Try Colour first (requires all-integer channels); fall through to vec3 on failure
                    Wayfinder::LinearColour colour = Wayfinder::LinearColour::White();
                    std::string unused;
                    if (ParseLinearColour(node, colour, unused))
                    {
                        block.SetColour(name, colour);
                    }
                    else if (node.size() == 3)
                    {
                        Wayfinder::Float3 v{
                            static_cast<float>(node[0].get<double>()),
                            static_cast<float>(node[1].get<double>()),
                            static_cast<float>(node[2].get<double>())};
                        block.SetVec3(name, v);
                    }
                    else if (node.size() == 4)
                    {
                        Wayfinder::Float4 v{
                            static_cast<float>(node[0].get<double>()),
                            static_cast<float>(node[1].get<double>()),
                            static_cast<float>(node[2].get<double>()),
                            static_cast<float>(node[3].get<double>())};
                        block.SetVec4(name, v);
                    }
                }
                else if (node.size() == 2)
                {
                    Wayfinder::Float2 v{
                        static_cast<float>(node[0].get<double>()),
                        static_cast<float>(node[1].get<double>())};
                    block.SetVec2(name, v);
                }
            }
            else if (node.is_number_float())
            {
                block.SetFloat(name, static_cast<float>(node.get<double>()));
            }
            else if (node.is_number_integer())
            {
                block.SetInt(name, static_cast<int32_t>(node.get<int64_t>()));
            }
        }
    }
}

namespace Wayfinder
{
    LinearColour MaterialAsset::GetBaseColour() const
    {
        auto it = Parameters.Values.find("base_colour");
        if (it != Parameters.Values.end())
        {
            if (const auto* c = std::get_if<LinearColour>(&it->second)) return *c;
        }
        return LinearColour::White();
    }

    void MaterialAsset::SetBaseColour(const LinearColour& colour)
    {
        Parameters.SetColour("base_colour", colour);
    }

    bool ParseMaterialAssetDocument(
        const nlohmann::json& document,
        const std::string& sourceLabel,
        MaterialAsset& material,
        std::string& error)
    {
        if (!document.contains(kAssetIdKey) || !document.at(kAssetIdKey).is_string())
        {
            error = "Material asset '" + sourceLabel + "' is missing asset_id";
            return false;
        }

        const std::string assetIdText = document.at(kAssetIdKey).get<std::string>();
        const std::optional<AssetId> assetId = AssetId::Parse(assetIdText);
        if (!assetId)
        {
            error = "Material asset '" + sourceLabel + "' has an invalid asset_id";
            return false;
        }

        if (!document.contains(kAssetTypeKey) || !document.at(kAssetTypeKey).is_string())
        {
            error = "Material asset '" + sourceLabel + "' is missing asset_type";
            return false;
        }

        const std::string assetType = document.at(kAssetTypeKey).get<std::string>();
        if (assetType != "material")
        {
            error = "Material asset '" + sourceLabel + "' must declare asset_type = 'material'";
            return false;
        }

        MaterialAsset parsed;
        parsed.Id = *assetId;
        parsed.Name = document.value(std::string{kNameKey}, std::filesystem::path(sourceLabel).stem().string());
        parsed.ShaderName = document.value(std::string{kShaderKey}, std::string("unlit"));

        // Parse "parameters" object if present
        if (document.contains(kParametersKey) && document.at(kParametersKey).is_object())
        {
            ParseParametersTable(document.at(kParametersKey), parsed.Parameters);
        }

        // Legacy support: top-level base_colour → parameters["base_colour"]
        // Only applied if parameters didn't already set it.
        if (!parsed.Parameters.Has("base_colour") && document.contains(kBaseColourKey))
        {
            LinearColour baseColour = LinearColour::White();
            std::string colourError;
            if (!ParseLinearColour(document.at(kBaseColourKey), baseColour, colourError))
            {
                error = "Material asset '" + sourceLabel + "' field 'base_colour' " + colourError;
                return false;
            }
            parsed.Parameters.SetColour("base_colour", baseColour);
        }

        // Default base_colour to white if nothing was specified
        if (!parsed.Parameters.Has("base_colour"))
        {
            parsed.Parameters.SetColour("base_colour", LinearColour::White());
        }

        // Parse "textures" object: maps slot name → texture asset ID string
        if (document.contains(kTexturesKey) && !document.at(kTexturesKey).is_object())
        {
            error = "Material asset '" + sourceLabel + "' has invalid 'textures' block: must be an object";
            return false;
        }

        if (document.contains(kTexturesKey) && document.at(kTexturesKey).is_object())
        {
            for (const auto& [slotName, idNode] : document.at(kTexturesKey).items())
            {
                if (!idNode.is_string())
                {
                    error = "Material asset '" + sourceLabel + "' textures['" + slotName + "'] must be a string (asset ID)";
                    return false;
                }

                const std::string idText = idNode.get<std::string>();
                const std::optional<AssetId> texAssetId = AssetId::Parse(idText);
                if (!texAssetId)
                {
                    error = "Material asset '" + sourceLabel + "' textures['" + slotName + "'] has an invalid asset ID: " + idText;
                    return false;
                }

                parsed.Textures[slotName] = *texAssetId;
            }
        }

        if (document.contains(kWireframeKey))
        {
            if (!document.at(kWireframeKey).is_boolean())
            {
                error = "Material asset '" + sourceLabel + "' field 'wireframe' must be a boolean";
                return false;
            }
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
            std::ifstream file(filePath.string());
            if (!file.is_open())
            {
                error = "Failed to open material asset '" + filePath.generic_string() + "'";
                return false;
            }
            const nlohmann::json document = nlohmann::json::parse(file);
            return ParseMaterialAssetDocument(document, filePath.generic_string(), material, error);
        }
        catch (const nlohmann::json::exception& parseError)
        {
            error = "Failed to parse material asset '" + filePath.generic_string() + "': " + parseError.what();
            return false;
        }
    }

    nlohmann::json CreateMaterialComponentTable(const MaterialAsset& material)
    {
        nlohmann::json table = nlohmann::json::object();
        table["material_id"] = material.Id.ToString();

        LinearColour baseColour = material.GetBaseColour();
        table["base_colour"] = nlohmann::json::array({
            static_cast<int64_t>(baseColour.r * 255.0f),
            static_cast<int64_t>(baseColour.g * 255.0f),
            static_cast<int64_t>(baseColour.b * 255.0f),
            static_cast<int64_t>(baseColour.a * 255.0f)
        });

        return table;
    }
} // namespace Wayfinder
