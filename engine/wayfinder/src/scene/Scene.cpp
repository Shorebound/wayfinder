#include "Scene.h"
#include "ComponentRegistry.h"
#include "SceneDocument.h"
#include "SceneModuleRegistry.h"
#include "entity/Entity.h"
#include "Components.h"
#include "../core/Log.h"

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
    Scene::Scene(const std::string& name) : m_name(name), m_initialized(false)
    {
    }

    Scene::~Scene()
    {
        if (m_initialized)
        {
            Shutdown();
        }
    }

    void Scene::Initialize()
    {
        WAYFINDER_INFO(LogScene, "Initializing scene: {0}", m_name);

        RegisterCoreComponents();
        RegisterCoreModules();

        m_initialized = true;
    }

    void Scene::RegisterCoreComponents()
    {
        m_world.component<SceneEntityComponent>();
        m_world.component<NameComponent>();
        m_world.component<SceneObjectIdComponent>();
        m_world.component<PrefabInstanceComponent>();
        SceneComponentRegistry::Get().RegisterComponents(m_world);
    }

    void Scene::RegisterCoreModules()
    {
        SceneModuleRegistry::Get().RegisterModules(m_world);
    }

    void Scene::ClearEntities()
    {
        std::vector<flecs::entity_t> entityIds;
        m_world.each([&](flecs::entity entityHandle)
        {
            if (!entityHandle.has<SceneEntityComponent>())
            {
                return;
            }

            entityIds.push_back(entityHandle.id());
        });

        for (const flecs::entity_t entityId : entityIds)
        {
            m_world.entity(entityId).destruct();
        }
    }

    void Scene::Update(float deltaTime)
    {
        if (!m_initialized) return;
        
        // Progress the Flecs ECS world
        m_world.progress(deltaTime);
    }

    void Scene::Shutdown()
    {
        WAYFINDER_INFO(LogScene, "Shutting down scene: {0}", m_name);
        
        // Flecs world gets destroyed automatically, but we can clear it if needed
        // m_world.quit();

        m_initialized = false;
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        std::string uniqueName = name;
        if (m_world.lookup(uniqueName.c_str()).is_valid())
        {
            uint32_t suffix = 1;
            while (m_world.lookup((name + std::to_string(suffix)).c_str()).is_valid())
            {
                ++suffix;
            }

            uniqueName = name + std::to_string(suffix);
        }

        flecs::entity handle = m_world.entity(uniqueName.c_str());
        handle.add<SceneEntityComponent>();
        handle.add<NameComponent>();
        handle.get_mut<NameComponent>().Value = uniqueName;
        handle.set<SceneObjectIdComponent>({SceneObjectId::Generate()});
        
        WAYFINDER_INFO(LogScene, "Created entity: {0} (ID: {1})", uniqueName, handle.id());
        
        return Entity{handle, this};
    }

    Entity Scene::GetEntityByName(const std::string& name)
    {
        flecs::entity handle = m_world.lookup(name.c_str());
        if (handle.is_valid())
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
            {
                return;
            }

            result = Entity{entityHandle, this};
        });

        return result;
    }

    bool Scene::LoadFromFile(const std::string& filePath)
    {
        if (!m_initialized)
        {
            WAYFINDER_ERROR(LogScene, "Scene must be initialized before loading data: {0}", filePath);
            return false;
        }

        try
        {
            const SceneDocumentLoadResult loadResult = LoadSceneDocument(filePath, SceneComponentRegistry::Get(), m_assetService.get());
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

                SceneComponentRegistry::Get().ApplyComponents(definition.ComponentData, entity);

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
        if (!m_initialized)
        {
            WAYFINDER_ERROR(LogScene, "Scene must be initialized before saving data: {0}", filePath);
            return false;
        }

        try
        {
            const SceneComponentRegistry& registry = SceneComponentRegistry::Get();
            SceneDocument document;
            document.Name = m_name;

            m_world.each([&](flecs::entity entityHandle)
            {
                if (!entityHandle.has<SceneEntityComponent>())
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

                registry.SerializeComponents(entity, record.ComponentData);
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
