#include "Material.h"

#include <fstream>

namespace
{
    const std::string kAssetIdKey = "asset_id";
    const std::string kAssetTypeKey = "asset_type";
    const std::string kNameKey = "name";
    const std::string kShaderKey = "shader";
    const std::string kBaseColorKey = "base_color";
    const std::string kWireframeKey = "wireframe";
    const std::string kParametersKey = "parameters";

    /// Parse a JSON array of 3 or 4 integers into a LinearColor.
    bool ParseLinearColor(const nlohmann::json& values, Wayfinder::LinearColor& color, std::string& error)
    {
        if (!values.is_array() || (values.size() != 3 && values.size() != 4))
        {
            error = "must be an array of 3 or 4 integers";
            return false;
        }

        auto readChannel = [&](size_t index, float fallback) -> float
        {
            if (!values[index].is_number_integer()) return fallback;
            return static_cast<float>(values[index].get<int64_t>()) / 255.0f;
        };

        color.r = readChannel(0, color.r);
        color.g = readChannel(1, color.g);
        color.b = readChannel(2, color.b);
        color.a = values.size() == 4 ? readChannel(3, color.a) : color.a;
        return true;
    }

    /// Parse a JSON "parameters" object into a MaterialParameterBlock.
    /// Supports: arrays of 3–4 numbers as Color, single numbers as Float, integers as Int.
    void ParseParametersTable(const nlohmann::json& params, Wayfinder::MaterialParameterBlock& block)
    {
        for (const auto& [key, node] : params.items())
        {
            const std::string name{key};

            if (node.is_array())
            {
                if (node.size() >= 3 && node.size() <= 4)
                {
                    // Treat as Color (integer RGBA → LinearColor)
                    Wayfinder::LinearColor color = Wayfinder::LinearColor::White();
                    std::string unused;
                    if (ParseLinearColor(node, color, unused))
                    {
                        block.SetColor(name, color);
                    }
                }
                else if (node.size() == 2)
                {
                    glm::vec2 v{
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
    LinearColor MaterialAsset::GetBaseColor() const
    {
        auto it = Parameters.Values.find("base_color");
        if (it != Parameters.Values.end())
        {
            if (const auto* c = std::get_if<LinearColor>(&it->second)) return *c;
        }
        return LinearColor::White();
    }

    void MaterialAsset::SetBaseColor(const LinearColor& color)
    {
        Parameters.SetColor("base_color", color);
    }

    bool ParseMaterialAssetDocument(
        const nlohmann::json& document,
        const std::string& sourceLabel,
        MaterialAsset& material,
        std::string& error)
    {
        if (!document.contains(kAssetIdKey) || !document[kAssetIdKey].is_string())
        {
            error = "Material asset '" + sourceLabel + "' is missing asset_id";
            return false;
        }

        const std::string assetIdText = document[kAssetIdKey].get<std::string>();
        const std::optional<AssetId> assetId = AssetId::Parse(assetIdText);
        if (!assetId)
        {
            error = "Material asset '" + sourceLabel + "' has an invalid asset_id";
            return false;
        }

        if (!document.contains(kAssetTypeKey) || !document[kAssetTypeKey].is_string())
        {
            error = "Material asset '" + sourceLabel + "' is missing asset_type";
            return false;
        }

        const std::string assetType = document[kAssetTypeKey].get<std::string>();
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
        if (document.contains(kParametersKey) && document[kParametersKey].is_object())
        {
            ParseParametersTable(document[kParametersKey], parsed.Parameters);
        }

        // Legacy support: top-level base_color → parameters["base_color"]
        // Only applied if parameters didn't already set it.
        if (!parsed.Parameters.Has("base_color") && document.contains(kBaseColorKey))
        {
            LinearColor baseColor = LinearColor::White();
            std::string colorError;
            if (!ParseLinearColor(document[kBaseColorKey], baseColor, colorError))
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

        if (document.contains(kWireframeKey))
        {
            if (!document[kWireframeKey].is_boolean())
            {
                error = "Material asset '" + sourceLabel + "' field 'wireframe' must be a boolean";
                return false;
            }
            parsed.Wireframe = document[kWireframeKey].get<bool>();
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
        table["shader"] = material.ShaderName;
        table["wireframe"] = material.Wireframe;

        // Serialize parameters as a nested object
        nlohmann::json paramsObj = nlohmann::json::object();
        for (const auto& [name, value] : material.Parameters.Values)
        {
            std::visit([&paramsObj, &name](auto&& v)
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, LinearColor>)
                {
                    paramsObj[name] = nlohmann::json::array({
                        static_cast<int64_t>(v.r * 255.0f),
                        static_cast<int64_t>(v.g * 255.0f),
                        static_cast<int64_t>(v.b * 255.0f),
                        static_cast<int64_t>(v.a * 255.0f)
                    });
                }
                else if constexpr (std::is_same_v<T, float>)
                {
                    paramsObj[name] = v;
                }
                else if constexpr (std::is_same_v<T, int32_t>)
                {
                    paramsObj[name] = v;
                }
                else if constexpr (std::is_same_v<T, glm::vec2>)
                {
                    paramsObj[name] = nlohmann::json::array({v.x, v.y});
                }
                else if constexpr (std::is_same_v<T, glm::vec3>)
                {
                    paramsObj[name] = nlohmann::json::array({v.x, v.y, v.z});
                }
                else if constexpr (std::is_same_v<T, glm::vec4>)
                {
                    paramsObj[name] = nlohmann::json::array({v.x, v.y, v.z, v.w});
                }
            }, value);
        }

        if (!paramsObj.empty())
        {
            table["parameters"] = std::move(paramsObj);
        }

        // Also write top-level base_color for backward compatibility
        LinearColor baseColor = material.GetBaseColor();
        table["base_color"] = nlohmann::json::array({
            static_cast<int64_t>(baseColor.r * 255.0f),
            static_cast<int64_t>(baseColor.g * 255.0f),
            static_cast<int64_t>(baseColor.b * 255.0f),
            static_cast<int64_t>(baseColor.a * 255.0f)
        });

        return table;
    }
} // namespace Wayfinder
