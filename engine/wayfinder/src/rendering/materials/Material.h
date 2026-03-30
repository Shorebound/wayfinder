#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "MaterialParameter.h"
#include "assets/AssetLoader.h"
#include "core/Identifiers.h"
#include "core/Result.h"
#include "core/Types.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    // ── Material Blend Mode ──────────────────────────────────
    // Authored blend preset stored in material JSON. GPU-agnostic — the
    // renderer maps this to a concrete BlendState at submission time.

    enum class MaterialBlendMode : uint8_t
    {
        Opaque = 0,
        AlphaBlend,
        Additive,
        Premultiplied,
        Multiplicative
    };

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
        MaterialBlendMode BlendMode = MaterialBlendMode::Opaque;
        MaterialParameterBlock Parameters;

        /// Named texture slot references: slot name (e.g. "diffuse") → texture AssetId.
        std::unordered_map<std::string, AssetId> Textures;

        // Convenience accessors for the most common parameter.
        LinearColour GetBaseColour() const;
        void SetBaseColour(const LinearColour& colour);
    };

    WAYFINDER_API Result<MaterialAsset> ParseMaterialAssetDocument(const nlohmann::json& document, const std::string& sourceLabel);

    WAYFINDER_API Result<MaterialAsset> LoadMaterialAssetFromFile(const std::filesystem::path& filePath);

    WAYFINDER_API nlohmann::json CreateMaterialComponentTable(const MaterialAsset& material);

    // ── AssetLoader specialisation ───────────────────────────
    // Allows MaterialAsset to be loaded through the generic AssetCache<T> path.
    /// @todo Refactor AssetLoader<T> and AssetCache<T> to return Result<TAsset>
    /// across the full asset pipeline in a follow-up PR. This adapter remains
    /// optional-based until mesh, texture, and cache loading share the same API.

    template<>
    struct AssetLoader<MaterialAsset>
    {
        static std::optional<MaterialAsset> Load(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error)
        {
            Result<MaterialAsset> materialResult = ParseMaterialAssetDocument(document, filePath.generic_string());
            if (!materialResult)
            {
                error = materialResult.error().GetMessage();
                return std::nullopt;
            }

            return std::move(materialResult.value());
        }
    };

} // namespace Wayfinder
