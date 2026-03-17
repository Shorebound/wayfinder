#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <toml++/toml.hpp>

#include "AssetRegistry.h"
#include "../graphics/Material.h"
#include "../scene/ComponentRegistry.h"

namespace Wayfinder
{
    class AssetSchemaRegistry
    {
    public:
        struct Entry
        {
            std::string_view TypeName;
            std::optional<AssetKind> BuiltinKind;
            bool (*ValidateFn)(const toml::table& document, const std::filesystem::path& filePath, std::string& error);
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
            const toml::table& document,
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
            static constexpr std::array<Entry, 2> entries = {{
                {"prefab", AssetKind::Prefab, &ValidatePrefabDocument},
                {"material", AssetKind::Material, &ValidateMaterialDocument},
            }};
            return entries;
        }

        static bool ValidatePrefabDocument(
            const toml::table& document,
            const std::filesystem::path& filePath,
            std::string& error)
        {
            const SceneComponentRegistry& registry = SceneComponentRegistry::Get();

            if (const toml::node* nameNode = document.get("name"); nameNode && !nameNode->is_string())
            {
                error = "Prefab asset '" + filePath.generic_string() + "' field 'name' must be a string";
                return false;
            }

            for (const auto& [key, node] : document)
            {
                const std::string_view keyView = key.str();
                if (keyView == "asset_id" || keyView == "asset_type" || keyView == "name")
                {
                    continue;
                }

                const toml::table* componentTable = node.as_table();
                if (!componentTable)
                {
                    error = "Prefab asset '" + filePath.generic_string() + "' field '" + std::string{keyView}
                        + "' must be a TOML table. Prefab payload is defined by component tables.";
                    return false;
                }

                if (!registry.IsRegistered(keyView))
                {
                    error = "Prefab asset '" + filePath.generic_string() + "' has unknown component table '" + std::string{keyView} + "'";
                    return false;
                }

                std::string validationError;
                if (!registry.ValidateComponent(keyView, *componentTable, validationError))
                {
                    error = "Prefab asset '" + filePath.generic_string() + "' component '" + std::string{keyView}
                        + "' is invalid: " + validationError;
                    return false;
                }
            }

            return true;
        }

        static bool ValidateMaterialDocument(
            const toml::table& document,
            const std::filesystem::path& filePath,
            std::string& error)
        {
            MaterialAsset material;
            return ParseMaterialAssetDocument(document, filePath.generic_string(), material, error);
        }
    };
} // namespace Wayfinder