#include "SceneDocument.h"

#include "../assets/AssetService.h"
#include "../graphics/Material.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace
{
    constexpr std::string_view kSceneNameKey = "scene_name";
    constexpr std::string_view kEntitiesKey = "entities";
    constexpr std::string_view kIdKey = "id";
    constexpr std::string_view kNameKey = "name";
    constexpr std::string_view kParentIdKey = "parent_id";
    constexpr std::string_view kPrefabIdKey = "prefab_id";
    constexpr std::string_view kAssetIdKey = "asset_id";
    constexpr std::string_view kAssetTypeKey = "asset_type";
    constexpr std::string_view kMeshComponentKey = "mesh";
    constexpr std::string_view kMaterialComponentKey = "material";
    constexpr std::string_view kRenderableComponentKey = "renderable";

    std::filesystem::path FindAssetRoot(const std::filesystem::path& filePath)
    {
        std::filesystem::path current = std::filesystem::weakly_canonical(filePath).parent_path();
        while (!current.empty())
        {
            if (current.filename() == "assets")
            {
                return current;
            }

            current = current.parent_path();
        }

        return {};
    }

    template <typename TId>
    std::optional<TId> ParseTypedId(
        const toml::table& table,
        std::string_view key,
        const std::string& sourceLabel,
        std::vector<std::string>& errors)
    {
        const toml::node* node = table.get(key);
        if (!node)
        {
            return std::nullopt;
        }

        if (!node->is_string())
        {
            errors.push_back(sourceLabel + " field '" + std::string{key} + "' must be a string");
            return std::nullopt;
        }

        const std::string text = node->value_or(std::string{});
        const std::optional<TId> parsed = TId::Parse(text);
        if (!parsed)
        {
            errors.push_back(sourceLabel + " field '" + std::string{key} + "' must be a valid UUID");
            return std::nullopt;
        }

        return parsed;
    }

    void AssignNode(toml::table& destination, std::string_view key, const toml::node& source)
    {
        const std::string keyString{key};

        if (const toml::table* value = source.as_table())
        {
            destination.insert_or_assign(keyString, *value);
            return;
        }

        if (const toml::array* value = source.as_array())
        {
            destination.insert_or_assign(keyString, *value);
            return;
        }

        if (const auto value = source.value<std::string>())
        {
            destination.insert_or_assign(keyString, *value);
            return;
        }

        if (const auto value = source.value<bool>())
        {
            destination.insert_or_assign(keyString, *value);
            return;
        }

        if (const auto value = source.value<int64_t>())
        {
            destination.insert_or_assign(keyString, *value);
            return;
        }

        if (const auto value = source.value<double>())
        {
            destination.insert_or_assign(keyString, *value);
        }
    }

    void MergeTables(toml::table& destination, const toml::table& overrides)
    {
        for (const auto& [key, node] : overrides)
        {
            const std::string_view keyView = key.str();
            if (const toml::table* sourceTable = node.as_table())
            {
                if (toml::table* destinationTable = destination.get_as<toml::table>(keyView))
                {
                    MergeTables(*destinationTable, *sourceTable);
                }
                else
                {
                    destination.insert_or_assign(std::string{keyView}, *sourceTable);
                }

                continue;
            }

            AssignNode(destination, keyView, node);
        }
    }

    Wayfinder::SceneDocumentEntity ParseEntityDefinition(
        const toml::table& table,
        const Wayfinder::SceneComponentRegistry& registry,
        const std::string& fallbackName,
        const std::string& sourceLabel,
        std::vector<std::string>& errors)
    {
        Wayfinder::SceneDocumentEntity definition;
        if (const std::optional<Wayfinder::SceneObjectId> parsedId = ParseTypedId<Wayfinder::SceneObjectId>(table, kIdKey, sourceLabel, errors))
        {
            definition.Id = *parsedId;
        }
        else
        {
            definition.Id = Wayfinder::SceneObjectId::Generate();
        }

        if (const auto name = table[kNameKey].value<std::string>())
        {
            definition.Name = *name;
        }
        else
        {
            definition.Name = fallbackName;
        }

        definition.ParentId = ParseTypedId<Wayfinder::SceneObjectId>(table, kParentIdKey, sourceLabel, errors);
        definition.PrefabAssetId = ParseTypedId<Wayfinder::AssetId>(table, kPrefabIdKey, sourceLabel, errors);

        for (const auto& [key, node] : table)
        {
            const std::string_view keyView = key.str();
            if (keyView == kIdKey || keyView == kNameKey || keyView == kParentIdKey || keyView == kPrefabIdKey || keyView == kAssetIdKey || keyView == kAssetTypeKey)
            {
                continue;
            }

            if (!registry.IsRegistered(keyView))
            {
                errors.push_back(sourceLabel + " has unknown component table '" + std::string{keyView} + "'");
                continue;
            }

            const toml::table* componentTable = node.as_table();
            if (!componentTable)
            {
                errors.push_back(sourceLabel + " component '" + std::string{keyView} + "' must be a TOML table");
                continue;
            }

            std::string validationError;
            if (!registry.ValidateComponent(keyView, *componentTable, validationError))
            {
                errors.push_back(sourceLabel + " component '" + std::string{keyView} + "' is invalid: " + validationError);
                continue;
            }

            definition.ComponentData.insert_or_assign(std::string{keyView}, *componentTable);
        }

        return definition;
    }

    std::optional<Wayfinder::SceneDocumentEntity> ParsePrefabDefinition(
        const std::filesystem::path& prefabPath,
        const Wayfinder::SceneComponentRegistry& registry,
        std::unordered_map<std::string, Wayfinder::SceneDocumentEntity>& prefabCache,
        std::vector<std::string>& errors)
    {
        const std::string key = std::filesystem::weakly_canonical(prefabPath).string();
        if (const auto cached = prefabCache.find(key); cached != prefabCache.end())
        {
            return cached->second;
        }

        try
        {
            toml::table prefabData = toml::parse_file(key);
            Wayfinder::SceneDocumentEntity definition = ParseEntityDefinition(
                prefabData,
                registry,
                prefabPath.stem().string(),
                "Prefab '" + prefabPath.generic_string() + "'",
                errors);
            prefabCache.emplace(key, definition);
            return definition;
        }
        catch (const toml::parse_error& error)
        {
            errors.push_back("Failed to parse prefab '" + prefabPath.generic_string() + "': " + std::string{error.description()});
            return std::nullopt;
        }
    }

    void MergeDefinition(Wayfinder::SceneDocumentEntity& destination, const Wayfinder::SceneDocumentEntity& overrideData)
    {
        if (!overrideData.Name.empty())
        {
            destination.Name = overrideData.Name;
        }
        if (!overrideData.Id.IsNil())
        {
            destination.Id = overrideData.Id;
        }
        if (overrideData.ParentId)
        {
            destination.ParentId = overrideData.ParentId;
        }
        if (overrideData.PrefabAssetId)
        {
            destination.PrefabAssetId = overrideData.PrefabAssetId;
        }

        MergeTables(destination.ComponentData, overrideData.ComponentData);
    }

    bool ResolveMaterialComponentData(
        Wayfinder::SceneDocumentEntity& definition,
        Wayfinder::AssetService& assetService,
        const std::string& sourceLabel,
        std::vector<std::string>& errors)
    {
        toml::table* materialTable = definition.ComponentData.get_as<toml::table>(kMaterialComponentKey);
        if (!materialTable)
        {
            return true;
        }

        const std::string materialLabel = sourceLabel + " material";
        const std::optional<Wayfinder::AssetId> materialAssetId = ParseTypedId<Wayfinder::AssetId>(
            *materialTable,
            "material_id",
            materialLabel,
            errors);
        if (!materialAssetId)
        {
            return true;
        }

        const Wayfinder::AssetRecord* materialRecord = assetService.ResolveRecord(*materialAssetId);
        if (!materialRecord)
        {
            errors.push_back(sourceLabel + " references missing material asset id '" + materialAssetId->ToString() + "'");
            return false;
        }

        if (materialRecord->Kind != Wayfinder::AssetKind::Material)
        {
            errors.push_back(
                sourceLabel + " references asset id '" + materialAssetId->ToString() + "' as a material, but it is registered as '"
                + materialRecord->TypeName + "'");
            return false;
        }

        std::string materialError;
        const Wayfinder::MaterialAsset* materialAsset = assetService.LoadMaterialAsset(*materialAssetId, materialError);
        if (!materialAsset)
        {
            errors.push_back(materialError);
            return false;
        }

        toml::table mergedMaterialTable = Wayfinder::CreateMaterialComponentTable(*materialAsset);
        MergeTables(mergedMaterialTable, *materialTable);
        *materialTable = std::move(mergedMaterialTable);
        return true;
    }

    bool ValidateRenderableRequirements(
        const Wayfinder::SceneDocumentEntity& definition,
        const std::string& sourceLabel,
        std::vector<std::string>& errors)
    {
        if (definition.ComponentData.contains(kMeshComponentKey) && !definition.ComponentData.contains(kRenderableComponentKey))
        {
            errors.push_back(sourceLabel + " has mesh data but no renderable component; renderability must now be explicit");
            return false;
        }

        return true;
    }
}

namespace Wayfinder
{
    SceneDocumentLoadResult LoadSceneDocument(const std::string& filePath, const SceneComponentRegistry& registry, AssetService* assetService)
    {
        SceneDocumentLoadResult result;

        try
        {
            const std::filesystem::path scenePath = std::filesystem::weakly_canonical(std::filesystem::path(filePath));
            const std::filesystem::path assetRoot = FindAssetRoot(scenePath);
            toml::table sceneData = toml::parse_file(filePath);
            AssetService localAssetService;
            AssetService& activeAssetService = assetService ? *assetService : localAssetService;
            std::string assetRegistryError;
            std::unordered_map<std::string, SceneDocumentEntity> prefabCache;
            SceneDocument document;
            std::unordered_set<std::string> entityNames;
            std::unordered_set<SceneObjectId> entityIds;

            if (!activeAssetService.SetAssetRoot(assetRoot, assetRegistryError))
            {
                result.Errors.push_back(assetRegistryError);
                return result;
            }

            document.Name = sceneData[kSceneNameKey].value_or(std::string{"Default Scene"});

            const toml::array* entities = sceneData[kEntitiesKey].as_array();
            if (!entities)
            {
                result.Errors.push_back("Scene file does not contain an entity list");
                return result;
            }

            size_t index = 0;
            for (const toml::node& entityNode : *entities)
            {
                const toml::table* entityTable = entityNode.as_table();
                if (!entityTable)
                {
                    result.Errors.push_back("Entity #" + std::to_string(index) + " must be a TOML table");
                    ++index;
                    continue;
                }

                const std::string entityLabel = "Entity #" + std::to_string(index);
                SceneDocumentEntity definition = ParseEntityDefinition(
                    *entityTable,
                    registry,
                    "Entity" + std::to_string(index),
                    entityLabel,
                    result.Errors);

                if (definition.PrefabAssetId)
                {
                    const AssetRecord* prefabRecord = activeAssetService.ResolveRecord(*definition.PrefabAssetId);
                    if (!prefabRecord)
                    {
                        result.Errors.push_back(entityLabel + " references missing prefab asset id '" + definition.PrefabAssetId->ToString() + "'");
                        ++index;
                        continue;
                    }

                    if (prefabRecord->Kind != AssetKind::Prefab)
                    {
                        result.Errors.push_back(
                            entityLabel + " references asset id '" + definition.PrefabAssetId->ToString() + "' as a prefab, but it is registered as '"
                            + prefabRecord->TypeName + "'");
                        ++index;
                        continue;
                    }

                    const std::optional<SceneDocumentEntity> prefabDefinition = ParsePrefabDefinition(prefabRecord->Path, registry, prefabCache, result.Errors);
                    if (!prefabDefinition)
                    {
                        ++index;
                        continue;
                    }

                    SceneDocumentEntity mergedDefinition = *prefabDefinition;
                    MergeDefinition(mergedDefinition, definition);
                    definition = std::move(mergedDefinition);
                }

                if (!ResolveMaterialComponentData(definition, activeAssetService, entityLabel, result.Errors))
                {
                    ++index;
                    continue;
                }

                if (!ValidateRenderableRequirements(definition, entityLabel, result.Errors))
                {
                    ++index;
                    continue;
                }

                if (!entityNames.insert(definition.Name).second)
                {
                    result.Errors.push_back(entityLabel + " resolves to duplicate entity name '" + definition.Name + "'");
                }

                if (!entityIds.insert(definition.Id).second)
                {
                    result.Errors.push_back(entityLabel + " resolves to duplicate entity id '" + definition.Id.ToString() + "'");
                }

                document.Entities.push_back(std::move(definition));
                ++index;
            }

            for (const SceneDocumentEntity& definition : document.Entities)
            {
                if (definition.ParentId && entityIds.find(*definition.ParentId) == entityIds.end())
                {
                    result.Errors.push_back("Entity '" + definition.Name + "' references missing parent id '" + definition.ParentId->ToString() + "'");
                }
            }

            if (result.Errors.empty())
            {
                result.Document = std::move(document);
            }
        }
        catch (const toml::parse_error& error)
        {
            result.Errors.push_back("Failed to parse scene file '" + filePath + "': " + std::string{error.description()});
        }

        return result;
    }

    bool SaveSceneDocument(const SceneDocument& document, const std::string& filePath, std::string& error)
    {
        try
        {
            const std::filesystem::path outputPath = std::filesystem::path(filePath);
            const std::filesystem::path outputDirectory = outputPath.has_parent_path() ? outputPath.parent_path() : std::filesystem::current_path();
            toml::table sceneData;
            toml::array entitiesArray;

            sceneData.insert_or_assign(std::string{kSceneNameKey}, document.Name);

            std::vector<SceneDocumentEntity> sortedEntities = document.Entities;
            std::sort(sortedEntities.begin(), sortedEntities.end(), [](const SceneDocumentEntity& left, const SceneDocumentEntity& right)
            {
                return left.Name < right.Name;
            });

            for (const SceneDocumentEntity& entityRecord : sortedEntities)
            {
                toml::table entityTable;
                entityTable.insert_or_assign(std::string{kIdKey}, entityRecord.Id.ToString());
                entityTable.insert_or_assign(std::string{kNameKey}, entityRecord.Name);

                if (entityRecord.ParentId)
                {
                    entityTable.insert_or_assign(std::string{kParentIdKey}, entityRecord.ParentId->ToString());
                }

                if (entityRecord.PrefabAssetId)
                {
                    entityTable.insert_or_assign(std::string{kPrefabIdKey}, entityRecord.PrefabAssetId->ToString());
                }

                MergeTables(entityTable, entityRecord.ComponentData);
                entitiesArray.push_back(std::move(entityTable));
            }

            sceneData.insert_or_assign(std::string{kEntitiesKey}, std::move(entitiesArray));

            std::filesystem::create_directories(outputDirectory);
            std::ofstream stream(outputPath, std::ios::out | std::ios::trunc | std::ios::binary);
            if (!stream.is_open())
            {
                error = "Failed to open scene output file for writing: " + filePath;
                return false;
            }

            stream << sceneData;
            stream.flush();

            if (!stream.good())
            {
                error = "Failed while writing scene data to: " + filePath;
                return false;
            }

            return true;
        }
        catch (const std::exception& exception)
        {
            error = "Failed to save scene file '" + filePath + "': " + exception.what();
            return false;
        }
    }
} // namespace Wayfinder