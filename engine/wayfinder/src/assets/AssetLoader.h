#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace Wayfinder
{
    /**
     * @brief Traits type that defines how a particular asset type is loaded from disk.
     *
     * Primary template is deleted — each asset type must provide an explicit
     * specialisation with:
     *
     *     static std::optional<T> Load(
     *         const nlohmann::json& document,
     *         const std::filesystem::path& filePath,
     *         std::string& error);
     */
    template<typename TAsset>
    struct AssetLoader
    {
        static_assert(sizeof(TAsset) == 0, "AssetLoader<T> must be specialised for each asset type.");
    };

} // namespace Wayfinder
