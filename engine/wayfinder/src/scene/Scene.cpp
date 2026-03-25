#include "Scene.h"
#include "Components.h"
#include "RuntimeComponentRegistry.h"
#include "SceneDocument.h"
#include "core/Log.h"
#include "project/ProjectResolver.h"
#include "scene/SceneSettings.h"
#include "scene/entity/Entity.h"

#include <filesystem>
#include <format>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Wayfinder
{
    namespace
    {
        void LogDocumentErrors(const std::vector<std::string>& errors, const std::string& filePath)
        {
            Wayfinder::LogScene.GetLogger()->LogFormat(Wayfinder::LogVerbosity::Error, "Scene validation failed for {0} with {1} issue(s)", filePath, errors.size());
            for (const std::string& error : errors)
            {
                Wayfinder::LogScene.GetLogger()->LogFormat(Wayfinder::LogVerbosity::Error, "  - {0}", error);
            }
        }

        /// Flecs debug-build locks pair **second** ids while queries reference them; \c ecs_delete on the
        /// scene tag can assert. We never delete tag entities; we \c ecs_clear and recycle them per
        /// \c flecs::world. We do **not** call \c world.progress() on shutdown — that would advance the
        /// whole pipeline one frame (systems, timers, etc.) for unrelated work.
        std::unordered_map<flecs::world_t*, std::vector<flecs::entity_t>> s_sceneTagFreeListByWorld;
        std::unordered_set<flecs::world_t*> s_sceneTagPoolFiniRegistered;

        void OnWorldFiniSceneTagPool(ecs_world_t* world, void* ctx)
        {
            (void)ctx;
            s_sceneTagFreeListByWorld.erase(world);
            s_sceneTagPoolFiniRegistered.erase(world);
        }

        void EnsureSceneTagPoolTracksWorldLifetime(flecs::world& world)
        {
            flecs::world_t* w = world.c_ptr();
            if (s_sceneTagPoolFiniRegistered.insert(w).second)
            {
                ecs_atfini(w, OnWorldFiniSceneTagPool, nullptr);
            }
        }

        flecs::entity AcquireSceneTag(flecs::world& world)
        {
            EnsureSceneTagPoolTracksWorldLifetime(world);
            flecs::world_t* w = world.c_ptr();
            auto& list = s_sceneTagFreeListByWorld[w];
            if (!list.empty())
            {
                const flecs::entity_t id = list.back();
                list.pop_back();
                return world.entity(id);
            }

            return world.entity();
        }

        void ReleaseSceneTag(flecs::world& world, flecs::entity tag)
        {
            if (!tag.is_valid())
            {
                return;
            }

            /// Scene shutdown may run while Flecs is deferring mutations from a stage. In that case
            /// the tag must not be returned to the free list yet, because queued SceneOwnership pair
            /// ops may still reference the id and Flecs only permits \\c ecs_merge on stages, not the
            /// root world handle. Clearing the tag is still safe, but deferred shutdown abandons tag
            /// recycling for that scene instance instead of reusing the id too early.
            if (world.is_deferred())
            {
                ecs_clear(world.c_ptr(), tag.id());
                return;
            }

            ecs_clear(world.c_ptr(), tag.id());
            s_sceneTagFreeListByWorld[world.c_ptr()].push_back(tag.id());
        }
    } // anonymous namespace
}

namespace Wayfinder
{
    Scene::Scene(flecs::world& world, const RuntimeComponentRegistry& componentRegistry, std::string name) : m_world(world), m_componentRegistry(componentRegistry), m_name(std::move(name))
    {
        m_sceneTag = AcquireSceneTag(m_world);
        m_ownedEntitiesQuery = m_world.query_builder<>().with<SceneOwnership>(m_sceneTag).build();
    }

    void Scene::RegisterEntityId(flecs::entity entityHandle, const SceneObjectId& id) const
    {
        if (!entityHandle.is_valid() || id.IsNil())
        {
            return;
        }

        m_entitiesById[id] = entityHandle.id();
    }

    void Scene::UnregisterEntityId(const SceneObjectId& id) const
    {
        if (id.IsNil())
        {
            return;
        }

        m_entitiesById.erase(id);
    }

    void Scene::UpdateEntityId(flecs::entity entityHandle, const SceneObjectId& previousId, const SceneObjectId& newId) const
    {
        if (previousId == newId)
        {
            RegisterEntityId(entityHandle, newId);
            return;
        }

        UnregisterEntityId(previousId);
        RegisterEntityId(entityHandle, newId);
    }

    void Scene::RegisterEntityName(flecs::entity entityHandle, const std::string& name) const
    {
        if (!entityHandle.is_valid() || name.empty())
        {
            return;
        }

        m_entitiesByName[name] = entityHandle.id();
    }

    void Scene::UnregisterEntityName(const std::string& name) const
    {
        if (name.empty())
        {
            return;
        }

        m_entitiesByName.erase(name);
    }

    void Scene::UpdateEntityName(flecs::entity entityHandle, const std::string& previousName, const std::string& newName) const
    {
        if (previousName == newName)
        {
            return;
        }

        UnregisterEntityName(previousName);
        RegisterEntityName(entityHandle, newName);
    }

    Scene::~Scene() // NOLINT(bugprone-exception-escape)
    {
        Shutdown();
    }

    void Scene::RegisterCoreComponents(flecs::world& world)
    {
        world.component<SceneEntityComponent>();
        world.component<SceneOwnership>();
        world.component<NameComponent>();
        world.component<SceneObjectIdComponent>();
        world.component<PrefabInstanceComponent>();
    }

    void Scene::ClearEntities(const bool rebuildOwnedEntitiesQuery)
    {
        /// Cached query must not outlive deletes — and must not be rebuilt on shutdown while \ref m_sceneTag
        /// still exists, or Flecs keeps the tag entity delete-locked for queries.
        m_ownedEntitiesQuery = flecs::query<>{};

        flecs::query<> collectQuery = m_world.query_builder<>().with<SceneOwnership>(m_sceneTag).build();
        std::vector<flecs::entity> owned;
        collectQuery.each([&](flecs::entity entityHandle)
        {
            if (entityHandle.is_valid())
            {
                owned.push_back(entityHandle);
            }
        });
        collectQuery = flecs::query<>{};

        for (flecs::entity entityHandle : owned)
        {
            entityHandle.destruct();
        }

        m_entitiesById.clear();
        m_entitiesByName.clear();

        // Reset scene-scoped settings
        m_world.set<SceneSettings>({});

        if (rebuildOwnedEntitiesQuery && m_sceneTag.is_valid())
        {
            m_ownedEntitiesQuery = m_world.query_builder<>().with<SceneOwnership>(m_sceneTag).build();
        }
    }

    void Scene::Shutdown()
    {
        if (!m_active)
        {
            return;
        }

        WAYFINDER_INFO(LogScene, "Shutting down scene: {0}", m_name);

        ClearEntities(false);

        /// Pair queries lock the tag id as a pair second (Flecs debug). \c ClearEntities destroys those
        /// queries. We do not \c ecs_delete the tag; we return it to the pool via \c ReleaseSceneTag.
        if (m_sceneTag.is_valid())
        {
            ReleaseSceneTag(m_world, m_sceneTag);
        }
        m_sceneTag = flecs::entity{};

        m_active = false;
    }

    bool Scene::IsNameTaken(const std::string& name, flecs::entity_t excludeEntity) const
    {
        const auto it = m_entitiesByName.find(name);
        return it != m_entitiesByName.end() && it->second != excludeEntity;
    }

    std::string Scene::GenerateUniqueName(const std::string& base, flecs::entity_t excludeEntity) const
    {
        if (!IsNameTaken(base, excludeEntity))
        {
            return base;
        }

        uint32_t suffix = 1;
        while (IsNameTaken(base + std::to_string(suffix), excludeEntity))
        {
            ++suffix;
        }

        return base + std::to_string(suffix);
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        std::string uniqueName = GenerateUniqueName(name);

        /// Create an anonymous flecs entity.  Entity identity within a
        /// scene is tracked via NameComponent / SceneObjectIdComponent,
        /// not flecs' world-global naming system.
        const flecs::entity handle = m_world.entity();
        const SceneObjectId sceneObjectId = SceneObjectId::Generate();
        handle.add<SceneEntityComponent>();
        handle.add<SceneOwnership>(m_sceneTag);
        handle.set<NameComponent>(NameComponent{uniqueName});
        handle.set<SceneObjectIdComponent>({sceneObjectId});
        RegisterEntityId(handle, sceneObjectId);
        RegisterEntityName(handle, uniqueName);

        WAYFINDER_INFO(LogScene, "Created entity: {0} (ID: {1})", uniqueName, handle.id());

        return Entity{handle, this};
    }

    Entity Scene::GetEntityByName(const std::string& name)
    {
        const auto nameIt = m_entitiesByName.find(name);
        if (nameIt == m_entitiesByName.end())
        {
            return {};
        }

        const flecs::entity entityHandle = m_world.entity(nameIt->second);
        if (!entityHandle.is_valid() || !entityHandle.has<SceneOwnership>(m_sceneTag) || !entityHandle.has<NameComponent>() || !(entityHandle.get<NameComponent>().Value == name))
        {
            m_entitiesByName.erase(nameIt);
            return {};
        }

        return Entity{entityHandle, this};
    }

    Entity Scene::GetEntityById(const SceneObjectId& id)
    {
        const auto entityIt = m_entitiesById.find(id);
        if (entityIt == m_entitiesById.end())
        {
            return {};
        }

        const flecs::entity entityHandle = m_world.entity(entityIt->second);
        if (!entityHandle.is_valid() || !entityHandle.has<SceneOwnership>(m_sceneTag) || !entityHandle.has<SceneObjectIdComponent>() || !(entityHandle.get<SceneObjectIdComponent>().Value == id))
        {
            m_entitiesById.erase(entityIt);
            return {};
        }

        return Entity{entityHandle, this};
    }

    Result<void> Scene::LoadFromFile(const std::string& filePath)
    {
        bool didMutateScene = false;
        const auto rollbackLoad = [this, &didMutateScene]()
        {
            if (!didMutateScene)
            {
                return;
            }

            ClearEntities();
            m_sourcePath.clear();
            m_assetRoot.clear();
        };

        try
        {
            const SceneDocumentLoadResult loadResult = LoadSceneDocument(filePath, m_componentRegistry, m_assetService.get());
            if (!loadResult.Document)
            {
                LogDocumentErrors(loadResult.Errors, filePath);
                return MakeError(std::format("Failed to load scene document: {}", filePath));
            }

            const std::filesystem::path sourcePath = std::filesystem::weakly_canonical(std::filesystem::path(filePath));
            const std::filesystem::path assetRoot = FindAssetRoot(sourcePath).value_or(std::filesystem::path{});

            ClearEntities();
            didMutateScene = true;

            std::unordered_map<SceneObjectId, Entity> createdEntitiesById;
            for (const SceneDocumentEntity& definition : loadResult.Document->Entities)
            {
                Entity entity = CreateEntity(definition.Name);
                if (const auto idResult = entity.SetSceneObjectId(definition.Id); !idResult)
                {
                    rollbackLoad();
                    return MakeError(std::format("Failed to set scene object id for '{}': {}", definition.Name, idResult.error().GetMessage()));
                }

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

            m_name = loadResult.Document->Name;
            m_sourcePath = sourcePath;
            m_assetRoot = assetRoot;

            WAYFINDER_INFO(LogScene, "Loaded scene data from: {0}", filePath);

            // Apply scene settings as a world singleton
            SceneSettings settings;
            settings.SetData(loadResult.Document->Settings);
            m_world.set<SceneSettings>(settings);

            return {};
        }
        catch (const std::exception& error)
        {
            rollbackLoad();
            WAYFINDER_ERROR(LogScene, "Failed to load scene file {0}: {1}", filePath, error.what());
            return MakeError(std::format("Failed to load scene file: {}", error.what()));
        }
    }

    Result<void> Scene::SaveToFile(const std::string& filePath) const
    {
        try
        {
            SceneDocument document;
            document.Name = m_name;

            // Persist scene settings from the world singleton into the document
            if (m_world.has<SceneSettings>())
            {
                document.Settings = m_world.get<SceneSettings>().GetData();
            }

            m_ownedEntitiesQuery.each([&](flecs::entity entityHandle)
            {
                const Entity entity{entityHandle, this};
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

                m_componentRegistry.SerialiseComponents(entity, record.ComponentData);
                document.Entities.push_back(std::move(record));
            });

            std::string error;
            if (!SaveSceneDocument(document, filePath, error))
            {
                WAYFINDER_ERROR(LogScene, "{0}", error);
                return MakeError(error);
            }

            WAYFINDER_INFO(LogScene, "Saved scene data to: {0}", filePath);
            return {};
        }
        catch (const std::exception& error)
        {
            WAYFINDER_ERROR(LogScene, "Failed to save scene file {0}: {1}", filePath, error.what());
            return MakeError(std::format("Failed to save scene file: {}", error.what()));
        }
    }

} // namespace Wayfinder
