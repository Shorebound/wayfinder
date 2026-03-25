#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "AssetRegistry.h"

namespace Wayfinder
{
    class AssetSchemaRegistry
    {
    public:
        struct Entry
        {
            std::string_view TypeName;
            std::optional<AssetKind> BuiltinKind;
            bool (*ValidateFn)(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error);
        };

        static bool IsRegisteredType(std::string_view typeName);
        static std::optional<AssetKind> ResolveBuiltinKind(std::string_view typeName);

        static bool ValidateDocument(std::string_view typeName, const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error);

    private:
        static const Entry* Find(std::string_view typeName);
        static const std::array<Entry, 4>& GetEntries();

        static bool ValidatePrefabDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error);

        static bool ValidateMaterialDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error);

        static bool ValidateTextureDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error);

        static bool ValidateMeshDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error);
    };
} // namespace Wayfinder