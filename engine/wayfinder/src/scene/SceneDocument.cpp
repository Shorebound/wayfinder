#include "SceneDocument.h"

#include "RuntimeComponentRegistry.h"
#include "assets/AssetService.h"
#include "project/ProjectResolver.h"
#include "rendering/materials/Material.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace Wayfinder
{
    namespace
    {
        constexpr std::string_view SCENE_NAME_KEY = "scene_name";
        constexpr std::string_view VERSION_KEY = "version";
        constexpr std::string_view SETTINGS_KEY = "settings";
        constexpr std::string_view ENTITIES_KEY = "entities";
        constexpr std::string_view ID_KEY = "id";
        constexpr std::string_view NAME_KEY = "name";
        constexpr std::string_view PARENT_ID_KEY = "parent_id";
        constexpr std::string_view PREFAB_ID_KEY = "prefab_id";
        constexpr std::string_view ASSET_ID_KEY = "asset_id";
        constexpr std::string_view ASSET_TYPE_KEY = "asset_type";
        constexpr std::string_view MESH_COMPONENT_KEY = "mesh";
        constexpr std::string_view MATERIAL_COMPONENT_KEY = "material";
        constexpr std::string_view RENDERABLE_COMPONENT_KEY = "renderable";

        template<typename TId>
        std::optional<TId> ParseTypedId(const nlohmann::json& data, std::string_view key, const std::string& sourceLabel, std::vector<std::string>& errors)
        {
            if (!data.contains(key))
            {
                return std::nullopt;
            }

            const auto& node = data.at(key);
            if (!node.is_string())
            {
                errors.push_back(std::format("{} field '{}' must be a string", sourceLabel, key));
                return std::nullopt;
            }

            const std::string text = node.get<std::string>();
            const std::optional<TId> parsed = TId::Parse(text);
            if (!parsed)
            {
                errors.push_back(std::format("{} field '{}' must be a valid UUID", sourceLabel, key));
                return std::nullopt;
            }

            return parsed;
        }

        // NOLINTBEGIN(misc-no-recursion, cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
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
        // NOLINTEND(misc-no-recursion, cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

        // NOLINTBEGIN(bugprone-easily-swappable-parameters)
        Wayfinder::SceneDocumentEntity ParseEntityDefinition(
            const nlohmann::json& data, const Wayfinder::RuntimeComponentRegistry& registry, const std::string& fallbackName, const std::string& sourceLabel, std::vector<std::string>& errors)
        {
            Wayfinder::SceneDocumentEntity definition;
            if (const auto parsedId = ParseTypedId<Wayfinder::SceneObjectId>(data, ID_KEY, sourceLabel, errors))
            {
                definition.Id = *parsedId;
            }
            else
            {
                definition.Id = Wayfinder::SceneObjectId::Generate();
            }

            definition.Name = data.value(NAME_KEY, fallbackName);
            definition.ParentId = ParseTypedId<Wayfinder::SceneObjectId>(data, PARENT_ID_KEY, sourceLabel, errors);
            definition.PrefabAssetId = ParseTypedId<Wayfinder::AssetId>(data, PREFAB_ID_KEY, sourceLabel, errors);

            for (const auto& [key, node] : data.items())
            {
                if (key == ID_KEY || key == NAME_KEY || key == PARENT_ID_KEY || key == PREFAB_ID_KEY || key == ASSET_ID_KEY || key == ASSET_TYPE_KEY)
                {
                    continue;
                }

                if (!registry.IsRegistered(key))
                {
                    errors.push_back(std::format("{} has unknown component table '{}'", sourceLabel, key));
                    continue;
                }

                if (!node.is_object())
                {
                    errors.push_back(std::format("{} component '{}' must be a JSON object", sourceLabel, key));
                    continue;
                }

                std::string validationError;
                if (!registry.ValidateComponent(key, node, validationError))
                {
                    errors.push_back(std::format("{} component '{}' is invalid: {}", sourceLabel, key, validationError));
                    continue;
                }

                definition.ComponentData[key] = node; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            }

            return definition;
        }
        // NOLINTEND(bugprone-easily-swappable-parameters)

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
                const nlohmann::json prefabData = nlohmann::json::parse(file);
                Wayfinder::SceneDocumentEntity definition = ParseEntityDefinition(prefabData, registry, prefabPath.stem().string(), std::format("Prefab '{}'", prefabPath.generic_string()), errors);
                prefabCache.emplace(key, definition);
                return definition;
            }
            catch (const nlohmann::json::exception& error)
            {
                errors.push_back(std::format("Failed to parse prefab '{}': {}", prefabPath.generic_string(), error.what()));
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
            if (!definition.ComponentData.contains(MATERIAL_COMPONENT_KEY) || !definition.ComponentData.at(MATERIAL_COMPONENT_KEY).is_object())
            {
                return true;
            }

            nlohmann::json& materialData = definition.ComponentData.at(MATERIAL_COMPONENT_KEY);
            const std::string materialLabel = sourceLabel + " material";
            const std::optional<Wayfinder::AssetId> materialAssetId = ParseTypedId<Wayfinder::AssetId>(materialData, "material_id", materialLabel, errors);
            if (!materialAssetId)
            {
                return true;
            }

            const Wayfinder::AssetRecord* materialRecord = assetService.ResolveRecord(*materialAssetId);
            if (!materialRecord)
            {
                errors.push_back(std::format("{} references missing material asset id '{}'", sourceLabel, materialAssetId->ToString()));
                return false;
            }

            if (materialRecord->Kind != Wayfinder::AssetKind::Material)
            {
                errors.push_back(std::format("{} references asset id '{}' as a material, but it is registered as '{}'", sourceLabel, materialAssetId->ToString(), materialRecord->TypeName));
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
            if (definition.ComponentData.contains(MESH_COMPONENT_KEY) && !definition.ComponentData.contains(RENDERABLE_COMPONENT_KEY))
            {
                errors.push_back(std::format("{} has mesh data but no renderable component; renderability must now be explicit", sourceLabel));
                return false;
            }

            return true;
        }
    } // anonymous namespace
} // namespace Wayfinder

namespace Wayfinder
{
    SceneDocumentLoadResult LoadSceneDocument(const std::string& filePath, const RuntimeComponentRegistry& registry, AssetService* assetService)
    {
        SceneDocumentLoadResult result;

        try
        {
            const std::filesystem::path scenePath = std::filesystem::weakly_canonical(std::filesystem::path(filePath));
            const std::filesystem::path assetRoot = FindAssetRoot(scenePath).value_or(std::filesystem::path{});

            std::ifstream file(filePath);
            if (!file.is_open())
            {
                result.Errors.push_back(std::format("Failed to open scene file '{}'", filePath));
                return result;
            }

            nlohmann::json sceneData = nlohmann::json::parse(file);

            if (!sceneData.contains(VERSION_KEY))
            {
                result.Errors.push_back(std::format("Scene file is missing required '{}' field", VERSION_KEY));
                return result;
            }

            if (!sceneData.at(VERSION_KEY).is_number_integer())
            {
                result.Errors.push_back(std::format("Scene '{}' must be an integer", VERSION_KEY));
                return result;
            }

            const int version = sceneData.at(VERSION_KEY).get<int>();
            if (version != SCENE_FORMAT_VERSION)
            {
                result.Errors.push_back(std::format("Unsupported scene format version {} (expected {})", version, SCENE_FORMAT_VERSION));
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

            document.Name = sceneData.value(SCENE_NAME_KEY, std::string{"Default Scene"});

            if (sceneData.contains(SETTINGS_KEY))
            {
                if (!sceneData.at(SETTINGS_KEY).is_object())
                {
                    result.Errors.push_back(std::format("'{}' must be an object", SETTINGS_KEY));
                    return result;
                }
                document.Settings = sceneData.at(SETTINGS_KEY);
            }

            if (!sceneData.contains(ENTITIES_KEY) || !sceneData.at(ENTITIES_KEY).is_array())
            {
                result.Errors.emplace_back("Scene file does not contain an entity list");
                return result;
            }

            const auto& entities = sceneData.at(ENTITIES_KEY);

            size_t index = 0;
            for (const auto& entityNode : entities)
            {
                if (!entityNode.is_object())
                {
                    result.Errors.push_back(std::format("Entity #{} must be a JSON object", index));
                    ++index;
                    continue;
                }

                const std::string entityLabel = std::format("Entity #{}", index);
                SceneDocumentEntity definition = ParseEntityDefinition(entityNode, registry, std::format("Entity{}", index), entityLabel, result.Errors);

                if (definition.PrefabAssetId)
                {
                    const AssetRecord* prefabRecord = activeAssetService.ResolveRecord(*definition.PrefabAssetId);
                    if (!prefabRecord)
                    {
                        result.Errors.push_back(std::format("{} references missing prefab asset id '{}'", entityLabel, definition.PrefabAssetId->ToString()));
                        ++index;
                        continue;
                    }

                    if (prefabRecord->Kind != AssetKind::Prefab)
                    {
                        result.Errors.push_back(std::format("{} references asset id '{}' as a prefab, but it is registered as '{}'", entityLabel, definition.PrefabAssetId->ToString(), prefabRecord->TypeName));
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
                    result.Errors.push_back(std::format("{} resolves to duplicate entity name '{}'", entityLabel, definition.Name));
                }

                if (!entityIds.insert(definition.Id).second)
                {
                    result.Errors.push_back(std::format("{} '{}' resolves to duplicate entity id '{}'", entityLabel, definition.Name, definition.Id.ToString()));
                }

                document.Entities.push_back(std::move(definition));
                ++index;
            }

            for (const SceneDocumentEntity& definition : document.Entities)
            {
                if (definition.ParentId && entityIds.find(*definition.ParentId) == entityIds.end())
                {
                    result.Errors.push_back(std::format("Entity '{}' references missing parent id '{}'", definition.Name, definition.ParentId->ToString()));
                }
            }

            if (result.Errors.empty())
            {
                result.Document = std::move(document);
            }
        }
        catch (const nlohmann::json::exception& error)
        {
            result.Errors.push_back(std::format("Failed to parse scene file '{}': {}", filePath, error.what()));
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

            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            sceneData[VERSION_KEY] = document.Version;
            sceneData[SCENE_NAME_KEY] = document.Name;

            if (!document.Settings.empty())
            {
                sceneData[SETTINGS_KEY] = document.Settings;
            }

            std::vector<SceneDocumentEntity> sortedEntities = document.Entities;
            std::ranges::sort(sortedEntities, [](const SceneDocumentEntity& left, const SceneDocumentEntity& right)
            {
                return left.Name < right.Name;
            });

            for (const SceneDocumentEntity& entityRecord : sortedEntities)
            {
                nlohmann::json entityObj = nlohmann::json::object();
                entityObj[ID_KEY] = entityRecord.Id.ToString();
                entityObj[NAME_KEY] = entityRecord.Name;

                if (entityRecord.ParentId)
                {
                    entityObj[PARENT_ID_KEY] = entityRecord.ParentId->ToString();
                }

                if (entityRecord.PrefabAssetId)
                {
                    entityObj[PREFAB_ID_KEY] = entityRecord.PrefabAssetId->ToString();
                }

                for (const auto& [key, value] : entityRecord.ComponentData.items())
                {
                    entityObj[key] = value;
                }

                entitiesArray.push_back(std::move(entityObj));
            }

            sceneData[ENTITIES_KEY] = std::move(entitiesArray);
            // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

            std::filesystem::create_directories(outputDirectory);
            std::ofstream stream(outputPath, std::ios::out | std::ios::trunc | std::ios::binary);
            if (!stream.is_open())
            {
                error = std::format("Failed to open scene output file for writing: {}", filePath);
                return false;
            }

            stream << sceneData.dump(2);
            stream.flush();

            if (!stream.good())
            {
                error = std::format("Failed while writing scene data to: {}", filePath);
                return false;
            }

            return true;
        }
        catch (const std::exception& exception)
        {
            error = std::format("Failed to save scene file '{}': {}", filePath, exception.what());
            return false;
        }
    }
} // namespace Wayfinder