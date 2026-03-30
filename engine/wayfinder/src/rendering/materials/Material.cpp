#include "Material.h"
#include "core/Log.h"
#include "core/Types.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>

namespace Wayfinder
{
    namespace
    {
        constexpr std::string_view ASSET_ID_KEY = "asset_id";
        constexpr std::string_view ASSET_TYPE_KEY = "asset_type";
        constexpr std::string_view NAME_KEY = "name";
        constexpr std::string_view SHADER_KEY = "shader";
        constexpr std::string_view BASE_COLOUR_KEY = "base_colour";
        constexpr std::string_view WIREFRAME_KEY = "wireframe";
        constexpr std::string_view PARAMETERS_KEY = "parameters";
        constexpr std::string_view TEXTURES_KEY = "textures";
        constexpr std::string_view BLEND_KEY = "blend";

        /// Tri-state result for ParseLinearColour.
        enum class ParseColourResult : uint8_t
        {
            NotColour, ///< Not a colour array (e.g. floats or wrong size) - caller may try vec fallback.
            Parsed,    ///< Successfully parsed as a valid colour.
            Invalid    ///< Looks like a colour (all integers) but values are out of range.
        };

        /// Parse a JSON array of 3 or 4 integers into a LinearColour.
        /// Returns NotColour when elements are not integers, Invalid when channels are out of range.
        /// @todo: move this to Result<LinearColour> probably
        ParseColourResult ParseLinearColour(const nlohmann::json& values, Wayfinder::LinearColour& colour, std::string& error)
        {
            if (!values.is_array() || (values.size() != 3 && values.size() != 4))
            {
                return ParseColourResult::NotColour;
            }

            for (size_t i = 0; i < values.size(); ++i)
            {
                if (!values[i].is_number_integer())
                {
                    return ParseColourResult::NotColour;
                }

                const auto channel = values[i].get<int64_t>();
                if (channel < 0 or channel > 255)
                {
                    error = std::format("channel {} value {} is out of range [0, 255]", i, channel);
                    return ParseColourResult::Invalid;
                }
            }

            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            colour.Data.r = static_cast<float>(values[0].get<int64_t>()) / 255.0f;
            colour.Data.g = static_cast<float>(values[1].get<int64_t>()) / 255.0f;
            colour.Data.b = static_cast<float>(values[2].get<int64_t>()) / 255.0f;
            colour.Data.a = values.size() == 4 ? static_cast<float>(values[3].get<int64_t>()) / 255.0f : colour.Data.a;
            // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return ParseColourResult::Parsed;
        }

        /// Parse a JSON "parameters" object into a MaterialParameterBlock.
        /// Supports: arrays of 3-4 numbers as Colour, single numbers as Float, integers as Int.
        /// Returns false and sets error when a parameter looks like a colour but has invalid channel values.
        bool ParseParametersTable(const nlohmann::json& params, Wayfinder::MaterialParameterBlock& block, std::string& error)
        {
            if (!params.is_object())
            {
                return true;
            }

            for (const auto& [key, node] : params.items())
            {
                const std::string name{key};

                if (node.is_array())
                {
                    if (node.size() >= 3 && node.size() <= 4)
                    {
                        // Try Colour first (requires all-integer channels); fall through to vec on NotColour.
                        Wayfinder::LinearColour colour = Wayfinder::LinearColour::White();
                        std::string colourError;
                        const auto colourResult = ParseLinearColour(node, colour, colourError);

                        if (colourResult == ParseColourResult::Parsed)
                        {
                            block.SetColour(name, colour);
                        }
                        else if (colourResult == ParseColourResult::Invalid)
                        {
                            error = std::format("parameter '{}': {}", name, colourError);
                            return false;
                        }
                        else if (node.size() == 3)
                        {
                            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                            const Wayfinder::Float3 v{static_cast<float>(node[0].get<double>()), static_cast<float>(node[1].get<double>()), static_cast<float>(node[2].get<double>())};
                            // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                            block.SetVec3(name, v);
                        }
                        else if (node.size() == 4)
                        {
                            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                            const Wayfinder::Float4 v{
                                static_cast<float>(node[0].get<double>()), static_cast<float>(node[1].get<double>()), static_cast<float>(node[2].get<double>()), static_cast<float>(node[3].get<double>())};
                            // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                            block.SetVec4(name, v);
                        }
                    }
                    else if (node.size() == 2)
                    {
                        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                        const Wayfinder::Float2 v{static_cast<float>(node[0].get<double>()), static_cast<float>(node[1].get<double>())};
                        // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
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

            return true;
        }

    } // namespace
}

namespace Wayfinder
{
    LinearColour MaterialAsset::GetBaseColour() const
    {
        auto it = Parameters.Values.find("base_colour");
        if (it != Parameters.Values.end())
        {
            if (const auto* c = std::get_if<LinearColour>(&it->second))
            {
                return *c;
            }
        }
        return LinearColour::White();
    }

    void MaterialAsset::SetBaseColour(const LinearColour& colour)
    {
        Parameters.SetColour("base_colour", colour);
    }

    bool ParseMaterialAssetDocument(const nlohmann::json& document, const std::string& sourceLabel, MaterialAsset& material, std::string& error)
    {
        if (!document.contains(ASSET_ID_KEY) || !document.at(ASSET_ID_KEY).is_string())
        {
            error = "Material asset '" + sourceLabel + "' is missing asset_id";
            return false;
        }

        const std::string assetIdText = document.at(ASSET_ID_KEY).get<std::string>();
        const std::optional<AssetId> assetId = AssetId::Parse(assetIdText);
        if (!assetId)
        {
            error = "Material asset '" + sourceLabel + "' has an invalid asset_id";
            return false;
        }

        if (!document.contains(ASSET_TYPE_KEY) || !document.at(ASSET_TYPE_KEY).is_string())
        {
            error = "Material asset '" + sourceLabel + "' is missing asset_type";
            return false;
        }

        const std::string assetType = document.at(ASSET_TYPE_KEY).get<std::string>();
        if (assetType != "material")
        {
            error = "Material asset '" + sourceLabel + "' must declare asset_type = 'material'";
            return false;
        }

        MaterialAsset parsed;
        parsed.Id = *assetId;
        parsed.Name = document.value(std::string{NAME_KEY}, std::filesystem::path(sourceLabel).stem().string());
        parsed.ShaderName = document.value(std::string{SHADER_KEY}, std::string("unlit"));

        // Parse "blend" preset (optional — defaults to Opaque)
        if (document.contains(BLEND_KEY))
        {
            if (!document.at(BLEND_KEY).is_string())
            {
                error = "Material asset '" + sourceLabel + "' field 'blend' must be a string";
                return false;
            }

            const std::string blendText = document.at(BLEND_KEY).get<std::string>();
            if (blendText == "alpha")
            {
                parsed.BlendMode = MaterialBlendMode::AlphaBlend;
            }
            else if (blendText == "additive")
            {
                parsed.BlendMode = MaterialBlendMode::Additive;
            }
            else if (blendText == "premultiplied")
            {
                parsed.BlendMode = MaterialBlendMode::Premultiplied;
            }
            else if (blendText == "multiplicative")
            {
                parsed.BlendMode = MaterialBlendMode::Multiplicative;
            }
            else if (blendText != "opaque")
            {
                error = "Material asset '" + sourceLabel + "' has unknown blend mode '" + blendText + "'";
                return false;
            }
        }

        // Parse "parameters" object if present
        if (document.contains(PARAMETERS_KEY) && document.at(PARAMETERS_KEY).is_object())
        {
            std::string paramError;
            if (!ParseParametersTable(document.at(PARAMETERS_KEY), parsed.Parameters, paramError))
            {
                error = "Material asset '" + sourceLabel + "' " + paramError;
                return false;
            }
        }

        // Legacy support: top-level base_colour → parameters["base_colour"]
        // Only applied if parameters didn't already set it.
        if (!parsed.Parameters.Has("base_colour") && document.contains(BASE_COLOUR_KEY))
        {
            LinearColour baseColour = LinearColour::White();
            std::string colourError;
            const auto colourResult = ParseLinearColour(document.at(BASE_COLOUR_KEY), baseColour, colourError);
            if (colourResult != ParseColourResult::Parsed)
            {
                error = "Material asset '" + sourceLabel + "' field 'base_colour' " + (colourResult == ParseColourResult::Invalid ? colourError : "must be an array of 3 or 4 integers [0-255]");
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
        if (document.contains(TEXTURES_KEY) && !document.at(TEXTURES_KEY).is_object())
        {
            error = "Material asset '" + sourceLabel + "' has invalid 'textures' block: must be an object";
            return false;
        }

        if (document.contains(TEXTURES_KEY) && document.at(TEXTURES_KEY).is_object())
        {
            for (const auto& [slotName, idNode] : document.at(TEXTURES_KEY).items())
            {
                if (!idNode.is_string())
                {
                    error = std::format("Material asset '{}' textures['{}'] must be a string (asset ID)", sourceLabel, slotName);
                    return false;
                }

                const std::string idText = idNode.get<std::string>();
                const std::optional<AssetId> texAssetId = AssetId::Parse(idText);
                if (!texAssetId)
                {
                    error = std::format("Material asset '{}' textures['{}'] has an invalid asset ID: {}", sourceLabel, slotName, idText);
                    return false;
                }

                parsed.Textures[slotName] = *texAssetId;
            }
        }

        if (document.contains(WIREFRAME_KEY))
        {
            if (!document.at(WIREFRAME_KEY).is_boolean())
            {
                error = "Material asset '" + sourceLabel + "' field 'wireframe' must be a boolean";
                return false;
            }
        }

        material = std::move(parsed);
        return true;
    }

    bool LoadMaterialAssetFromFile(const std::filesystem::path& filePath, MaterialAsset& material, std::string& error)
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
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        table["material_id"] = material.Id.ToString();

        const LinearColour baseColour = material.GetBaseColour();
        const auto toChannel = [](float v) -> int64_t
        {
            return std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
        };
        table["base_colour"] = nlohmann::json::array({toChannel(baseColour.Data.r), toChannel(baseColour.Data.g), toChannel(baseColour.Data.b), toChannel(baseColour.Data.a)});
        // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

        return table;
    }
} // namespace Wayfinder
