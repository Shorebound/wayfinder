#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "assets/AssetLoader.h"
#include "core/Identifiers.h"
#include "MaterialParameter.h"
#include "core/Types.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    // ── Material Asset ───────────────────────────────────────
    // The authored, disk-backed material definition (loaded from JSON).
    // Carries a shader reference and a generic parameter bag.
    // Specific parameters (base_colour, roughness, etc.) are stored
    // in the parameter block — the struct has no fixed fields per shader.

    struct WAYFINDER_API MaterialAsset
    {
        AssetId Id;
        std::string Name;
        std::string ShaderName = "unlit";
        MaterialParameterBlock Parameters;

        /// Named texture slot references: slot name (e.g. "diffuse") → texture AssetId.
        std::unordered_map<std::string, AssetId> Textures;

        // Convenience accessors for the most common parameter.
        LinearColour GetBaseColour() const;
        void SetBaseColour(const LinearColour& colour);
    };

    WAYFINDER_API bool ParseMaterialAssetDocument(
        const nlohmann::json& document,
        const std::string& sourceLabel,
        MaterialAsset& material,
        std::string& error);

    WAYFINDER_API bool LoadMaterialAssetFromFile(
        const std::filesystem::path& filePath,
        MaterialAsset& material,
        std::string& error);

    WAYFINDER_API nlohmann::json CreateMaterialComponentTable(const MaterialAsset& material);

    // ── AssetLoader specialisation ───────────────────────────
    // Allows MaterialAsset to be loaded through the generic AssetCache<T> path.

    template<>
    struct AssetLoader<MaterialAsset>
    {
        static std::optional<MaterialAsset> Load(
            const nlohmann::json& document,
            const std::filesystem::path& filePath,
            std::string& error)
        {
            MaterialAsset material;
            if (!ParseMaterialAssetDocument(document, filePath.generic_string(), material, error))
            {
                return std::nullopt;
            }
            return material;
        }
    };

} // namespace Wayfinder
