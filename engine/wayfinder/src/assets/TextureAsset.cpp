#include "TextureAsset.h"
#include "core/Log.h"

#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

namespace Wayfinder
{
    namespace
    {
        constexpr std::string_view TEXTURE_ASSET_ID_KEY = "asset_id";
        constexpr std::string_view TEXTURE_ASSET_TYPE_KEY = "asset_type";
        constexpr std::string_view TEXTURE_NAME_KEY = "name";
        constexpr std::string_view TEXTURE_SOURCE_KEY = "source";
        constexpr std::string_view TEXTURE_FILTER_KEY = "filter";
        constexpr std::string_view TEXTURE_ADDRESS_MODE_KEY = "address_mode";

        SamplerFilter ParseFilter(const std::string& text)
        {
            if (text == "nearest")
            {
                return SamplerFilter::Nearest;
            }
            return SamplerFilter::Linear; // default
        }

        SamplerAddressMode ParseAddressMode(const std::string& text)
        {
            if (text == "clamp" || text == "clamp_to_edge")
            {
                return SamplerAddressMode::ClampToEdge;
            }
            if (text == "mirrored_repeat")
            {
                return SamplerAddressMode::MirroredRepeat;
            }
            return SamplerAddressMode::Repeat; // default
        }
    }

    bool ValidateTextureAssetDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error)
    {
        const std::string label = filePath.generic_string();

        if (!document.contains(TEXTURE_ASSET_ID_KEY) || !document.at(TEXTURE_ASSET_ID_KEY).is_string())
        {
            error = "Texture asset '" + label + "' is missing asset_id";
            return false;
        }

        if (!document.contains(TEXTURE_ASSET_TYPE_KEY) || !document.at(TEXTURE_ASSET_TYPE_KEY).is_string())
        {
            error = "Texture asset '" + label + "' is missing asset_type";
            return false;
        }

        if (document.at(TEXTURE_ASSET_TYPE_KEY).get<std::string>() != "texture")
        {
            error = "Texture asset '" + label + "' must declare asset_type = 'texture'";
            return false;
        }

        if (!document.contains(TEXTURE_SOURCE_KEY) || !document.at(TEXTURE_SOURCE_KEY).is_string())
        {
            error = "Texture asset '" + label + "' is missing 'source' — the path to the image file";
            return false;
        }

        if (document.contains(TEXTURE_FILTER_KEY) && !document.at(TEXTURE_FILTER_KEY).is_string())
        {
            error = "Texture asset '" + label + R"(' field 'filter' must be a string ("nearest" or "linear"))";
            return false;
        }

        if (document.contains(TEXTURE_ADDRESS_MODE_KEY) && !document.at(TEXTURE_ADDRESS_MODE_KEY).is_string())
        {
            error = "Texture asset '" + label + R"(' field 'address_mode' must be a string ("repeat", "clamp", "mirrored_repeat"))";
            return false;
        }

        return true;
    }

    std::optional<TextureAsset> LoadTextureAssetFromDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error)
    {
        const std::string label = filePath.generic_string();

        // Validate structure first
        if (!ValidateTextureAssetDocument(document, filePath, error))
        {
            return std::nullopt;
        }

        // Parse asset ID
        const std::string assetIdText = document.at(TEXTURE_ASSET_ID_KEY).get<std::string>();
        const std::optional<AssetId> assetId = AssetId::Parse(assetIdText);
        if (!assetId)
        {
            error = "Texture asset '" + label + "' has an invalid asset_id";
            return std::nullopt;
        }

        TextureAsset texture;
        texture.Id = *assetId;
        texture.Name = document.value(std::string{TEXTURE_NAME_KEY}, filePath.stem().string());
        texture.SourcePath = document.at(TEXTURE_SOURCE_KEY).get<std::string>();
        texture.Filter = ParseFilter(document.value(std::string{TEXTURE_FILTER_KEY}, std::string("linear")));
        texture.AddressMode = ParseAddressMode(document.value(std::string{TEXTURE_ADDRESS_MODE_KEY}, std::string("repeat")));

        // Resolve image path relative to the JSON descriptor's directory
        const std::filesystem::path imageDir = filePath.parent_path();
        const std::filesystem::path imagePath = imageDir / texture.SourcePath;

        if (!std::filesystem::exists(imagePath))
        {
            error = "Texture asset '" + label + "' references image '" + imagePath.generic_string() + "' which does not exist";
            return std::nullopt;
        }

        // Load via SDL_image → SDL_Surface (forced to RGBA8)
        SDL_Surface* rawSurface = IMG_Load(imagePath.string().c_str());
        if (!rawSurface)
        {
            error = "Failed to load image '" + imagePath.generic_string() + "': " + std::string(SDL_GetError());
            return std::nullopt;
        }

        // Convert to RGBA8 if not already
        SDL_Surface* rgbaSurface = SDL_ConvertSurface(rawSurface, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(rawSurface);

        if (!rgbaSurface)
        {
            error = "Failed to convert image '" + imagePath.generic_string() + "' to RGBA: " + std::string(SDL_GetError());
            return std::nullopt;
        }

        texture.Width = static_cast<uint32_t>(rgbaSurface->w);
        texture.Height = static_cast<uint32_t>(rgbaSurface->h);
        texture.Channels = 4;

        const size_t pixelBytes = static_cast<size_t>(texture.Width) * texture.Height * texture.Channels;
        texture.PixelData.resize(pixelBytes);

        // Copy row-by-row to handle potential pitch differences
        const auto* src = static_cast<const uint8_t*>(rgbaSurface->pixels);
        const uint32_t dstPitch = texture.Width * texture.Channels;
        for (uint32_t row = 0; row < texture.Height; ++row)
        {
            const size_t dstOffset = static_cast<size_t>(row) * static_cast<size_t>(dstPitch);
            const size_t srcOffset = static_cast<size_t>(row) * static_cast<size_t>(rgbaSurface->pitch);
            std::memcpy(texture.PixelData.data() + dstOffset, src + srcOffset, dstPitch);
        }

        SDL_DestroySurface(rgbaSurface);

        WAYFINDER_INFO(LogAssets, "Loaded texture '{}' ({}x{}, RGBA8) from '{}'", texture.Name, texture.Width, texture.Height, imagePath.generic_string());

        return texture;
    }

} // namespace Wayfinder
