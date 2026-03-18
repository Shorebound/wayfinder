#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <toml++/toml.hpp>

#include "../core/Identifiers.h"
#include "RenderTypes.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    struct WAYFINDER_API MaterialAsset
    {
        AssetId Id;
        std::string Name;
        Color BaseColor = Color::White();
        bool Wireframe = true;
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
