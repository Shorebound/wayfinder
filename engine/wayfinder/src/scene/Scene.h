#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "ecs/Flecs.h"

#include "core/Identifiers.h"
#include "core/Result.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class AssetService;
    class Entity;
    class RuntimeComponentRegistry;

    class WAYFINDER_API Scene
    {
    public:
        Scene(flecs::world& world, const RuntimeComponentRegistry& componentRegistry, std::string name = "Default Scene");
        ~Scene();

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) = delete;
        Scene& operator=(Scene&&) = delete;

        /**
         * @brief Registers core scene infrastructure Flecs component types (identity, ownership, names, prefab links, etc.).
         *
         * Runtime components owned by scene plugins (e.g. world transform, active camera) are
         * registered via \ref PluginRegistry::RegisterComponent (RegisterFn only).
         *
         * @param world  Flecs world to register types on.
         */
        static void RegisterCoreComponents(flecs::world& world);

        void Shutdown();

        Entity CreateEntity(const std::string& name = "Entity");
        Entity GetEntityByName(const std::string& name);
        Entity GetEntityById(const SceneObjectId& id);

        /// Returns true if @p name is already taken by an entity in this scene
        /// other than @p excludeEntity.
        bool IsNameTaken(const std::string& name, flecs::entity_t excludeEntity = 0) const;

        /// Returns a scene-unique variant of @p base, appending a numeric suffix
        /// if necessary.  The entity identified by @p excludeEntity (if any)
        /// is ignored during the collision check.
        std::string GenerateUniqueName(const std::string& base, flecs::entity_t excludeEntity = 0) const;
        Result<void> LoadFromFile(const std::string& filePath);
        Result<void> SaveToFile(const std::string& filePath) const;
        void SetAssetService(const std::shared_ptr<AssetService>& assetService)
        {
            m_assetService = assetService;
        }
        const std::shared_ptr<AssetService>& GetAssetService() const
        {
            return m_assetService;
        }

        const std::string& GetName() const
        {
            return m_name;
        }
        const std::filesystem::path& GetSourcePath() const
        {
            return m_sourcePath;
        }
        const std::filesystem::path& GetAssetRoot() const
        {
            return m_assetRoot;
        }

        /// Expose the Flecs world for querying and external registration.
        flecs::world& GetWorld()
        {
            return m_world;
        }
        const flecs::world& GetWorld() const
        {
            return m_world;
        }

        /// Flecs entity id used as the \ref SceneOwnership pair target (scene identity). Stable for this
        /// \ref Scene instance until \ref Shutdown (then the id may be recycled for another \ref Scene).
        flecs::entity_t GetSceneTagEntityId() const noexcept
        {
            return m_sceneTag.id();
        }

    private:
        friend class Entity;

        /// Clears scene entities. When \p rebuildOwnedEntitiesQuery is false (shutdown path), the cached
        /// ownership query is not rebuilt so the scene tag can be released to the per-world pool without
        /// Flecs query locks on the pair target.
        void ClearEntities(bool rebuildOwnedEntitiesQuery = true);
        void RegisterEntityId(flecs::entity entityHandle, const SceneObjectId& id) const;
        void UnregisterEntityId(const SceneObjectId& id) const;
        void UpdateEntityId(flecs::entity entityHandle, const SceneObjectId& previousId, const SceneObjectId& newId) const;
        void RegisterEntityName(flecs::entity entityHandle, const std::string& name) const;
        void UnregisterEntityName(const std::string& name) const;
        void UpdateEntityName(flecs::entity entityHandle, const std::string& previousName, const std::string& newName) const;

        flecs::entity GetSceneTag() const
        {
            return m_sceneTag;
        }

        flecs::world& m_world;
        const RuntimeComponentRegistry& m_componentRegistry;
        flecs::entity m_sceneTag;
        /// Cached query: entities with \ref SceneOwnership for this scene (single source of truth for scene membership).
        mutable flecs::query<> m_ownedEntitiesQuery;
        mutable std::unordered_map<SceneObjectId, flecs::entity_t> m_entitiesById;
        mutable std::unordered_map<std::string, flecs::entity_t> m_entitiesByName;
        std::string m_name;
        std::filesystem::path m_sourcePath;
        std::filesystem::path m_assetRoot;
        std::shared_ptr<AssetService> m_assetService;
        bool m_active = true;
    };
} // namespace Wayfinder
