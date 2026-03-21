#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "AssetLoader.h"
#include "core/Identifiers.h"
#include "rendering/RenderTypes.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    /**
     * @brief CPU-side texture asset loaded from a JSON descriptor + image file.
     *
     * The JSON descriptor specifies metadata (filter, address mode) and
     * references a source image file. The pixel data is loaded via SDL_image
     * into an RGBA8 buffer for GPU upload.
     */
    struct WAYFINDER_API TextureAsset
    {
        AssetId Id;
        std::string Name;

        /// Relative path to the source image file (from asset root).
        std::string SourcePath;

        /// Sampling parameters (authored in the JSON descriptor).
        SamplerFilter Filter = SamplerFilter::Linear;
        SamplerAddressMode AddressMode = SamplerAddressMode::Repeat;

        /// Populated after image load.
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Channels = 4; // Always forced to RGBA.
        std::vector<uint8_t> PixelData;
    };

    /// Parse and validate a texture asset JSON document (schema check only, no image load).
    WAYFINDER_API bool ValidateTextureAssetDocument(
        const nlohmann::json& document,
        const std::filesystem::path& filePath,
        std::string& error);

    /// Parse a texture JSON and load the referenced image from disk via SDL_image.
    WAYFINDER_API std::optional<TextureAsset> LoadTextureAssetFromDocument(
        const nlohmann::json& document,
        const std::filesystem::path& filePath,
        std::string& error);

    // ── AssetLoader specialisation ───────────────────────────

    template<>
    struct AssetLoader<TextureAsset>
    {
        static std::optional<TextureAsset> Load(
            const nlohmann::json& document,
            const std::filesystem::path& filePath,
            std::string& error)
        {
            return LoadTextureAssetFromDocument(document, filePath, error);
        }
    };

} // namespace Wayfinder
