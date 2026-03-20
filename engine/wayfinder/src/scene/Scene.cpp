#include "Scene.h"
#include "RuntimeComponentRegistry.h"
#include "SceneDocument.h"
#include "SceneModuleRegistry.h"
#include "scene/entity/Entity.h"
#include "Components.h"
#include "core/Log.h"
#include "core/SceneSettings.h"

#include <filesystem>
#include <unordered_map>

namespace
{
    std::filesystem::path FindAssetRootFromScenePath(const std::filesystem::path& filePath)
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

    void LogDocumentErrors(const std::vector<std::string>& errors, const std::string& filePath)
    {
        Wayfinder::LogScene.GetLogger()->LogFormat(
            Wayfinder::LogVerbosity::Error,
            "Scene validation failed for {0} with {1} issue(s)",
            filePath,
            errors.size());
        for (const std::string& error : errors)
        {
            Wayfinder::LogScene.GetLogger()->LogFormat(Wayfinder::LogVerbosity::Error, "  - {0}", error);
        }
    }

}

namespace Wayfinder
{
    Scene::Scene(flecs::world& world, const RuntimeComponentRegistry& componentRegistry, const std::string& name)
        : m_world(world), m_componentRegistry(componentRegistry), m_name(name)
    {
        m_sceneTag = m_world.entity();
    }

    Scene::~Scene()
    {
        Shutdown();
    }

    void Scene::RegisterCoreECS(flecs::world& world)
    {
        world.component<SceneEntityComponent>();
        world.component<SceneOwnership>();
        world.component<NameComponent>();
        world.component<SceneObjectIdComponent>();
        world.component<PrefabInstanceComponent>();
        SceneModuleRegistry::Get().RegisterModules(world);
    }

    void Scene::ClearEntities()
    {
        std::vector<flecs::entity_t> entityIds;
        m_world.each([&](flecs::entity entityHandle)
        {
            if (!entityHandle.has<SceneOwnership>(m_sceneTag))
            {
                return;
            }

            entityIds.push_back(entityHandle.id());
        });

        for (const flecs::entity_t entityId : entityIds)
        {
            m_world.entity(entityId).destruct();
        }

        // Reset scene-scoped settings
        m_world.set<SceneSettings>({});
    }

    void Scene::Shutdown()
    {
        if (!m_active) return;

        WAYFINDER_INFO(LogScene, "Shutting down scene: {0}", m_name);
        
        ClearEntities();

        if (m_sceneTag.is_valid())
        {
            m_sceneTag.destruct();
        }

        m_active = false;
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        /// Helper: check whether a flecs name is already taken by an entity
        /// that belongs to *this* scene.  World-level lookup is still used
        /// (flecs names are world-global), but we only consider it a
        /// collision when the existing entity carries our SceneOwnership tag.
        auto nameOwnedByThisScene = [&](const char* candidate) -> bool
        {
            flecs::entity existing = m_world.lookup(candidate);
            return existing.is_valid() && existing.has<SceneOwnership>(m_sceneTag);
        };

        std::string uniqueName = name;
        if (nameOwnedByThisScene(uniqueName.c_str()))
        {
            uint32_t suffix = 1;
            while (nameOwnedByThisScene((name + std::to_string(suffix)).c_str()))
            {
                ++suffix;
            }

            uniqueName = name + std::to_string(suffix);
        }

        flecs::entity handle = m_world.entity(uniqueName.c_str());
        handle.add<SceneEntityComponent>();
        handle.add<SceneOwnership>(m_sceneTag);
        handle.add<NameComponent>();
        handle.get_mut<NameComponent>().Value = uniqueName;
        handle.set<SceneObjectIdComponent>({SceneObjectId::Generate()});
        
        WAYFINDER_INFO(LogScene, "Created entity: {0} (ID: {1})", uniqueName, handle.id());
        
        return Entity{handle, this};
    }

    Entity Scene::GetEntityByName(const std::string& name)
    {
        flecs::entity handle = m_world.lookup(name.c_str());
        if (handle.is_valid() && handle.has<SceneOwnership>(m_sceneTag))
        {
            return Entity{handle, this};
        }
        
        // Return an invalid entity wrapper if not found
        return Entity{};
    }

    Entity Scene::GetEntityById(const SceneObjectId& id)
    {
        Entity result;
        m_world.each([&](flecs::entity entityHandle, const SceneObjectIdComponent& sceneObjectId)
        {
            if (result || !(sceneObjectId.Value == id))
                return;

            if (!entityHandle.has<SceneOwnership>(m_sceneTag))
                return;

            result = Entity{entityHandle, this};
        });

        return result;
    }

    bool Scene::LoadFromFile(const std::string& filePath)
    {
        try
        {
            const SceneDocumentLoadResult loadResult = LoadSceneDocument(filePath, m_componentRegistry, m_assetService.get());
            if (!loadResult.Document)
            {
                LogDocumentErrors(loadResult.Errors, filePath);
                return false;
            }

            ClearEntities();
            m_name = loadResult.Document->Name;
            m_sourcePath = std::filesystem::weakly_canonical(std::filesystem::path(filePath));
            m_assetRoot = FindAssetRootFromScenePath(m_sourcePath);

            std::unordered_map<SceneObjectId, Entity> createdEntitiesById;
            for (const SceneDocumentEntity& definition : loadResult.Document->Entities)
            {
                Entity entity = CreateEntity(definition.Name);
                entity.SetName(definition.Name);
                entity.SetSceneObjectId(definition.Id);

                m_componentRegistry.ApplyComponents(definition.ComponentData, entity);

                if (!entity.HasComponent<TransformComponent>())
                {
                    entity.AddComponent<TransformComponent>(TransformComponent{});
                }

                if (definition.PrefabAssetId)
                {
                    entity.SetPrefabAssetId(*definition.PrefabAssetId);
                }

                createdEntitiesById.emplace(definition.Id, entity);
            }

            for (const SceneDocumentEntity& definition : loadResult.Document->Entities)
            {
                if (!definition.ParentId)
                {
                    continue;
                }

                const auto childIt = createdEntitiesById.find(definition.Id);
                const auto parentIdIt = createdEntitiesById.find(*definition.ParentId);
                if (childIt == createdEntitiesById.end() || parentIdIt == createdEntitiesById.end())
                {
                    WAYFINDER_WARNING(LogScene, "Could not resolve hierarchy link {0} -> {1}", definition.Name, definition.ParentId->ToString());
                    continue;
                }

                childIt->second.GetHandle().child_of(parentIdIt->second.GetHandle());
            }

            WAYFINDER_INFO(LogScene, "Loaded scene data from: {0}", filePath);

            // Apply scene settings as a world singleton
            SceneSettings settings;
            settings.SetData(loadResult.Document->Settings);
            m_world.set<SceneSettings>(settings);

            return true;
        }
        catch (const std::exception& error)
        {
            WAYFINDER_ERROR(LogScene, "Failed to load scene file {0}: {1}", filePath, error.what());
            return false;
        }
    }

    bool Scene::SaveToFile(const std::string& filePath) const
    {
        try
        {
            SceneDocument document;
            document.Name = m_name;

            // Persist scene settings from the world singleton into the document
            if (m_world.has<SceneSettings>())
                document.Settings = m_world.get<SceneSettings>().GetData();

            m_world.each([&](flecs::entity entityHandle)
            {
                if (!entityHandle.has<SceneOwnership>(m_sceneTag))
                {
                    return;
                }

                Entity entity{entityHandle, this};
                if (!entity.HasSceneObjectId())
                {
                    WAYFINDER_WARNING(LogScene, "Skipping scene entity without SceneObjectId during save: {0}", entity.GetName());
                    return;
                }

                SceneDocumentEntity record;
                record.Id = entity.GetSceneObjectId();
                record.Name = entity.GetName();

                const flecs::entity parentHandle = entityHandle.parent();
                if (parentHandle.is_valid())
                {
                    const Entity parentEntity{parentHandle, this};
                    if (parentEntity.HasSceneObjectId())
                    {
                        record.ParentId = parentEntity.GetSceneObjectId();
                    }
                }

                if (entity.HasPrefabAssetId())
                {
                    record.PrefabAssetId = entity.GetPrefabAssetId();
                }

                m_componentRegistry.SerializeComponents(entity, record.ComponentData);
                document.Entities.push_back(std::move(record));
            });

            std::string error;
            if (!SaveSceneDocument(document, filePath, error))
            {
                WAYFINDER_ERROR(LogScene, "{0}", error);
                return false;
            }

            WAYFINDER_INFO(LogScene, "Saved scene data to: {0}", filePath);
            return true;
        }
        catch (const std::exception& error)
        {
            WAYFINDER_ERROR(LogScene, "Failed to save scene file {0}: {1}", filePath, error.what());
            return false;
        }
    }

} // namespace Wayfinder
