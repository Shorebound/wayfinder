#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <toml++/toml.hpp>

#include "../core/Identifiers.h"
#include "MaterialParameter.h"
#include "RenderTypes.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    // ── Material Asset ───────────────────────────────────────
    // The authored, disk-backed material definition (loaded from TOML).
    // Carries a shader reference and a generic parameter bag.
    // Specific parameters (base_color, roughness, etc.) are stored
    // in the parameter block — the struct has no fixed fields per shader.

    struct WAYFINDER_API MaterialAsset
    {
        AssetId Id;
        std::string Name;
        std::string ShaderName = "unlit";
        MaterialParameterBlock Parameters;

        // Convenience accessors for the most common parameter.
        LinearColor GetBaseColor() const;
        void SetBaseColor(const LinearColor& colour);
    };

    WAYFINDER_API bool ParseMaterialAssetDocument(
        const toml::table& document,
        const std::string& sourceLabel,
        MaterialAsset& material,
        std::string& error);

    WAYFINDER_API bool LoadMaterialAssetFromFile(
        const std::filesystem::path& filePath,
        MaterialAsset& material,
        std::string& error);

    WAYFINDER_API toml::table CreateMaterialComponentTable(const MaterialAsset& material);
} // namespace Wayfinder
