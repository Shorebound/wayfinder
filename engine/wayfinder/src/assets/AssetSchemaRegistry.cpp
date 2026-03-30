#include "AssetSchemaRegistry.h"

#include "MeshAsset.h"
#include "TextureAsset.h"
#include "rendering/materials/Material.h"
#include "scene/ComponentRegistry.h"

namespace Wayfinder
{
    namespace
    {
        constexpr std::string_view NAME_KEY = "name";
        constexpr std::string_view ASSET_ID_KEY = "asset_id";
        constexpr std::string_view ASSET_TYPE_KEY = "asset_type";
    }

    bool AssetSchemaRegistry::IsRegisteredType(std::string_view typeName)
    {
        return Find(typeName) != nullptr;
    }

    std::optional<AssetKind> AssetSchemaRegistry::ResolveBuiltinKind(std::string_view typeName)
    {
        const Entry* entry = Find(typeName);
        if (!entry)
        {
            return std::nullopt;
        }

        return entry->BuiltinKind;
    }

    bool AssetSchemaRegistry::ValidateDocument(std::string_view typeName, const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error)
    {
        const Entry* entry = Find(typeName);
        if (!entry)
        {
            error = "Asset '" + filePath.generic_string() + "' has unsupported asset_type '" + std::string{typeName} + "'.";
            return false;
        }

        return entry->ValidateFn(document, filePath, error);
    }

    const AssetSchemaRegistry::Entry* AssetSchemaRegistry::Find(std::string_view typeName)
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

    const std::array<AssetSchemaRegistry::Entry, 4>& AssetSchemaRegistry::GetEntries()
    {
        static const std::array<Entry, 4> ENTRIES = {{
            {.TypeName = "prefab", .BuiltinKind = AssetKind::Prefab, .ValidateFn = &ValidatePrefabDocument},
            {.TypeName = "material", .BuiltinKind = AssetKind::Material, .ValidateFn = &ValidateMaterialDocument},
            {.TypeName = "texture", .BuiltinKind = AssetKind::Texture, .ValidateFn = &ValidateTextureDocument},
            {.TypeName = "mesh", .BuiltinKind = AssetKind::Mesh, .ValidateFn = &ValidateMeshDocument},
        }};
        return ENTRIES;
    }

    bool AssetSchemaRegistry::ValidatePrefabDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error)
    {
        const SceneComponentRegistry& registry = SceneComponentRegistry::Get();

        if (const auto nameIt = document.find(NAME_KEY); nameIt != document.end() && !nameIt->is_string())
        {
            error = "Prefab asset '" + filePath.generic_string() + "' field 'name' must be a string";
            return false;
        }

        for (const auto& [key, node] : document.items())
        {
            if (key == ASSET_ID_KEY || key == ASSET_TYPE_KEY || key == NAME_KEY)
            {
                continue;
            }

            if (!node.is_object())
            {
                error = "Prefab asset '" + filePath.generic_string() + "' field '" + key + "' must be a JSON object. Prefab payload is defined by component objects.";
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
                error = "Prefab asset '";
                error.append(filePath.generic_string());
                error.append("' component '");
                error.append(key);
                error.append("' is invalid: ");
                error.append(validationError);
                return false;
            }
        }

        return true;
    }

    bool AssetSchemaRegistry::ValidateMaterialDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error)
    {
        Result<MaterialAsset> materialResult = ParseMaterialAssetDocument(document, filePath.generic_string());
        if (materialResult)
        {
            return true;
        }

        error = materialResult.error().GetMessage();
        return false;
    }

    bool AssetSchemaRegistry::ValidateTextureDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error)
    {
        return ValidateTextureAssetDocument(document, filePath, error);
    }

    bool AssetSchemaRegistry::ValidateMeshDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error)
    {
        if (!ValidateMeshAssetDocument(document, filePath, error))
        {
            return false;
        }

        return ValidateMeshAssetBinary(document, filePath, error);
    }

} // namespace Wayfinder
