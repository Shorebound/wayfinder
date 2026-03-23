#include "SceneDocument.h"

#include "RuntimeComponentRegistry.h"
#include "assets/AssetService.h"
#include "project/ProjectResolver.h"
#include "rendering/materials/Material.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace Wayfinder
{
    const std::string kSceneNameKey = "scene_name";
    const std::string kVersionKey = "version";
    const std::string kSettingsKey = "settings";
    const std::string kEntitiesKey = "entities";
    const std::string kIdKey = "id";
    const std::string kNameKey = "name";
    const std::string kParentIdKey = "parent_id";
    const std::string kPrefabIdKey = "prefab_id";
    const std::string kAssetIdKey = "asset_id";
    const std::string kAssetTypeKey = "asset_type";
    const std::string kMeshComponentKey = "mesh";
    const std::string kMaterialComponentKey = "material";
    const std::string kRenderableComponentKey = "renderable";

    template<typename TId>
    std::optional<TId> ParseTypedId(const nlohmann::json& data, const std::string& key, const std::string& sourceLabel, std::vector<std::string>& errors)
    {
        if (!data.contains(key))
        {
            return std::nullopt;
        }

        const auto& node = data[key];
        if (!node.is_string())
        {
            errors.push_back(sourceLabel + " field '" + key + "' must be a string");
            return std::nullopt;
        }

        const std::string text = node.get<std::string>();
        const std::optional<TId> parsed = TId::Parse(text);
        if (!parsed)
        {
            errors.push_back(sourceLabel + " field '" + key + "' must be a valid UUID");
            return std::nullopt;
        }

        return parsed;
    }

    void MergeObjects(nlohmann::json& destination, const nlohmann::json& overrides)
    {
        for (const auto& [key, value] : overrides.items())
        {
            if (value.is_object() && destination.contains(key) && destination[key].is_object())
            {
                MergeObjects(destination[key], value);
            }
            else
            {
                destination[key] = value;
            }
        }
    }

    Wayfinder::SceneDocumentEntity ParseEntityDefinition(
    const nlohmann::json& data, const Wayfinder::RuntimeComponentRegistry& registry, const std::string& fallbackName, const std::string& sourceLabel, std::vector<std::string>& errors)
    {
        Wayfinder::SceneDocumentEntity definition;
        if (const auto parsedId = ParseTypedId<Wayfinder::SceneObjectId>(data, kIdKey, sourceLabel, errors))
        {
            definition.Id = *parsedId;
        }
        else
        {
            definition.Id = Wayfinder::SceneObjectId::Generate();
        }

        definition.Name = data.value(kNameKey, fallbackName);
        definition.ParentId = ParseTypedId<Wayfinder::SceneObjectId>(data, kParentIdKey, sourceLabel, errors);
        definition.PrefabAssetId = ParseTypedId<Wayfinder::AssetId>(data, kPrefabIdKey, sourceLabel, errors);

        for (const auto& [key, node] : data.items())
        {
            if (key == kIdKey || key == kNameKey || key == kParentIdKey || key == kPrefabIdKey || key == kAssetIdKey || key == kAssetTypeKey)
            {
                continue;
            }

            if (!registry.IsRegistered(key))
            {
                errors.push_back(sourceLabel + " has unknown component table '" + key + "'");
                continue;
            }

            if (!node.is_object())
            {
                errors.push_back(sourceLabel + " component '" + key + "' must be a JSON object");
                continue;
            }

            std::string validationError;
            if (!registry.ValidateComponent(key, node, validationError))
            {
                errors.push_back(sourceLabel + " component '" + key + "' is invalid: " + validationError);
                continue;
            }

            definition.ComponentData[key] = node;
        }

        return definition;
    }

    std::optional<Wayfinder::SceneDocumentEntity> ParsePrefabDefinition(
    const std::filesystem::path& prefabPath, const Wayfinder::RuntimeComponentRegistry& registry, std::unordered_map<std::string, Wayfinder::SceneDocumentEntity>& prefabCache, std::vector<std::string>& errors)
    {
        const std::string key = std::filesystem::weakly_canonical(prefabPath).string();
        if (const auto cached = prefabCache.find(key); cached != prefabCache.end())
        {
            return cached->second;
        }

        try
        {
            std::ifstream file(key);
            nlohmann::json prefabData = nlohmann::json::parse(file);
            Wayfinder::SceneDocumentEntity definition = ParseEntityDefinition(prefabData, registry, prefabPath.stem().string(), "Prefab '" + prefabPath.generic_string() + "'", errors);
            prefabCache.emplace(key, definition);
            return definition;
        }
        catch (const nlohmann::json::exception& error)
        {
            errors.push_back("Failed to parse prefab '" + prefabPath.generic_string() + "': " + error.what());
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

        MergeObjects(destination.ComponentData, overrideData.ComponentData);
    }

    bool ResolveMaterialComponentData(Wayfinder::SceneDocumentEntity& definition, Wayfinder::AssetService& assetService, const std::string& sourceLabel, std::vector<std::string>& errors)
    {
        if (!definition.ComponentData.contains(kMaterialComponentKey) || !definition.ComponentData[kMaterialComponentKey].is_object())
        {
            return true;
        }

        nlohmann::json& materialData = definition.ComponentData[kMaterialComponentKey];
        const std::string materialLabel = sourceLabel + " material";
        const std::optional<Wayfinder::AssetId> materialAssetId = ParseTypedId<Wayfinder::AssetId>(materialData, "material_id", materialLabel, errors);
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
            errors.push_back(sourceLabel + " references asset id '" + materialAssetId->ToString() + "' as a material, but it is registered as '" + materialRecord->TypeName + "'");
            return false;
        }

        std::string materialError;
        const Wayfinder::MaterialAsset* materialAsset = assetService.LoadMaterialAsset(*materialAssetId, materialError);
        if (!materialAsset)
        {
            errors.push_back(materialError);
            return false;
        }

        nlohmann::json mergedMaterialData = Wayfinder::CreateMaterialComponentTable(*materialAsset);
        MergeObjects(mergedMaterialData, materialData);
        materialData = std::move(mergedMaterialData);
        return true;
    }

    bool ValidateRenderableRequirements(const Wayfinder::SceneDocumentEntity& definition, const std::string& sourceLabel, std::vector<std::string>& errors)
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
    SceneDocumentLoadResult LoadSceneDocument(const std::string& filePath, const RuntimeComponentRegistry& registry, AssetService* assetService)
    {
        SceneDocumentLoadResult result;

        try
        {
            const std::filesystem::path scenePath = std::filesystem::weakly_canonical(std::filesystem::path(filePath));
            const std::filesystem::path assetRoot = Wayfinder::FindAssetRoot(scenePath).value_or(std::filesystem::path{});

            std::ifstream file(filePath);
            if (!file.is_open())
            {
                result.Errors.push_back("Failed to open scene file '" + filePath + "'");
                return result;
            }

            nlohmann::json sceneData = nlohmann::json::parse(file);

            if (!sceneData.contains(kVersionKey))
            {
                result.Errors.push_back("Scene file is missing required '" + kVersionKey + "' field");
                return result;
            }

            if (!sceneData[kVersionKey].is_number_integer())
            {
                result.Errors.push_back("Scene '" + kVersionKey + "' must be an integer");
                return result;
            }

            const int version = sceneData[kVersionKey].get<int>();
            if (version != SCENE_FORMAT_VERSION)
            {
                result.Errors.push_back("Unsupported scene format version " + std::to_string(version) + " (expected " + std::to_string(SCENE_FORMAT_VERSION) + ")");
                return result;
            }

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

            document.Name = sceneData.value(kSceneNameKey, std::string{"Default Scene"});

            if (sceneData.contains(kSettingsKey))
            {
                if (!sceneData[kSettingsKey].is_object())
                {
                    result.Errors.push_back("'" + std::string{kSettingsKey} + "' must be an object");
                    return result;
                }
                document.Settings = sceneData[kSettingsKey];
            }

            if (!sceneData.contains(kEntitiesKey) || !sceneData[kEntitiesKey].is_array())
            {
                result.Errors.push_back("Scene file does not contain an entity list");
                return result;
            }

            const auto& entities = sceneData[kEntitiesKey];

            size_t index = 0;
            for (const auto& entityNode : entities)
            {
                if (!entityNode.is_object())
                {
                    result.Errors.push_back("Entity #" + std::to_string(index) + " must be a JSON object");
                    ++index;
                    continue;
                }

                const std::string entityLabel = "Entity #" + std::to_string(index);
                SceneDocumentEntity definition = ParseEntityDefinition(entityNode, registry, "Entity" + std::to_string(index), entityLabel, result.Errors);

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
                        result.Errors.push_back(entityLabel + " references asset id '" + definition.PrefabAssetId->ToString() + "' as a prefab, but it is registered as '" + prefabRecord->TypeName + "'");
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
                    result.Errors.push_back(entityLabel + " '" + definition.Name + "' resolves to duplicate entity id '" + definition.Id.ToString() + "'");
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
        catch (const nlohmann::json::exception& error)
        {
            result.Errors.push_back("Failed to parse scene file '" + filePath + "': " + error.what());
        }

        return result;
    }

    bool SaveSceneDocument(const SceneDocument& document, const std::string& filePath, std::string& error)
    {
        try
        {
            const std::filesystem::path outputPath = std::filesystem::path(filePath);
            const std::filesystem::path outputDirectory = outputPath.has_parent_path() ? outputPath.parent_path() : std::filesystem::current_path();
            nlohmann::json sceneData = nlohmann::json::object();
            nlohmann::json entitiesArray = nlohmann::json::array();

            sceneData[kVersionKey] = document.Version;
            sceneData[kSceneNameKey] = document.Name;

            if (!document.Settings.empty())
            {
                sceneData[kSettingsKey] = document.Settings;
            }

            std::vector<SceneDocumentEntity> sortedEntities = document.Entities;
            std::sort(sortedEntities.begin(), sortedEntities.end(), [](const SceneDocumentEntity& left, const SceneDocumentEntity& right)
            {
                return left.Name < right.Name;
            });

            for (const SceneDocumentEntity& entityRecord : sortedEntities)
            {
                nlohmann::json entityObj = nlohmann::json::object();
                entityObj[kIdKey] = entityRecord.Id.ToString();
                entityObj[kNameKey] = entityRecord.Name;

                if (entityRecord.ParentId)
                {
                    entityObj[kParentIdKey] = entityRecord.ParentId->ToString();
                }

                if (entityRecord.PrefabAssetId)
                {
                    entityObj[kPrefabIdKey] = entityRecord.PrefabAssetId->ToString();
                }

                for (const auto& [key, value] : entityRecord.ComponentData.items())
                {
                    entityObj[key] = value;
                }

                entitiesArray.push_back(std::move(entityObj));
            }

            sceneData[kEntitiesKey] = std::move(entitiesArray);

            std::filesystem::create_directories(outputDirectory);
            std::ofstream stream(outputPath, std::ios::out | std::ios::trunc | std::ios::binary);
            if (!stream.is_open())
            {
                error = "Failed to open scene output file for writing: " + filePath;
                return false;
            }

            stream << sceneData.dump(2);
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