#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "AssetRegistry.h"
#include "rendering/materials/Material.h"
#include "scene/ComponentRegistry.h"

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

        static bool IsRegisteredType(const std::string_view typeName)
        {
            return Find(typeName) != nullptr;
        }

        static std::optional<AssetKind> ResolveBuiltinKind(const std::string_view typeName)
        {
            const Entry* entry = Find(typeName);
            if (!entry)
            {
                return std::nullopt;
            }

            return entry->BuiltinKind;
        }

        static bool ValidateDocument(
            const std::string_view typeName,
            const nlohmann::json& document,
            const std::filesystem::path& filePath,
            std::string& error)
        {
            const Entry* entry = Find(typeName);
            if (!entry)
            {
                error = "Asset '" + filePath.generic_string() + "' has unsupported asset_type '" + std::string{typeName} + "'.";
                return false;
            }

            return entry->ValidateFn(document, filePath, error);
        }

    private:
        static const Entry* Find(const std::string_view typeName)
        {
            for (const Entry& entry : GetEntries())
            {
                if (entry.TypeName == typeName)
                {
                    return &entry;
                }
            }

            return nullptr;
        }

        static const std::array<Entry, 2>& GetEntries()
        {
            static const std::array<Entry, 2> entries = {{
                {"prefab", AssetKind::Prefab, &ValidatePrefabDocument},
                {"material", AssetKind::Material, &ValidateMaterialDocument},
            }};
            return entries;
        }

        static bool ValidatePrefabDocument(
            const nlohmann::json& document,
            const std::filesystem::path& filePath,
            std::string& error)
        {
            const SceneComponentRegistry& registry = SceneComponentRegistry::Get();

            if (document.contains("name") && !document["name"].is_string())
            {
                error = "Prefab asset '" + filePath.generic_string() + "' field 'name' must be a string";
                return false;
            }

            for (const auto& [key, node] : document.items())
            {
                if (key == "asset_id" || key == "asset_type" || key == "name")
                {
                    continue;
                }

                if (!node.is_object())
                {
                    error = "Prefab asset '" + filePath.generic_string() + "' field '" + key
                        + "' must be a JSON object. Prefab payload is defined by component objects.";
                    return false;
                }

                if (!registry.IsRegistered(key))
                {
                    error = "Prefab asset '" + filePath.generic_string() + "' has unknown component table '" + key + "'";
                    return false;
                }

                std::string validationError;
                if (!registry.ValidateComponent(key, node, validationError))
                {
                    error = "Prefab asset '" + filePath.generic_string() + "' component '" + key
                        + "' is invalid: " + validationError;
                    return false;
                }
            }

            return true;
        }

        static bool ValidateMaterialDocument(
            const nlohmann::json& document,
            const std::filesystem::path& filePath,
            std::string& error)
        {
            MaterialAsset material;
            return ParseMaterialAssetDocument(document, filePath.generic_string(), material, error);
        }
    };
} // namespace Wayfinder