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

        /// Registers core scene infrastructure component types (identity, ownership, names, etc.).
        /// Runtime components owned by scene plugins (e.g. world transform, active camera) are
        /// registered via \ref PluginRegistry::RegisterComponent (RegisterFn only).
        /// Call once per world before creating any scenes.
        static void RegisterCoreComponents(flecs::world& world);

        /// Registers transform/camera Flecs types and simulation systems (same plugins as a game
        /// would add via its root plugin). Call after \ref RegisterCoreComponents (and after
        /// \ref RuntimeComponentRegistry::RegisterComponents when using scene JSON), before
        /// \c world.progress(), when tests or tools need the same systems as the running game.
        static void RegisterCoreSceneSystems(flecs::world& world);

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

    private:
        friend class Entity;

        void ClearEntities();
        void RegisterEntityId(flecs::entity entityHandle, const SceneObjectId& id) const;
        void UnregisterEntityId(const SceneObjectId& id) const;
        void UpdateEntityId(flecs::entity entityHandle, const SceneObjectId& previousId, const SceneObjectId& newId) const;
        void RegisterEntityName(flecs::entity entityHandle, const std::string& name) const;
        void UnregisterEntityName(const std::string& name) const;
        void UpdateEntityName(flecs::entity entityHandle, const std::string& previousName, const std::string& newName) const;

        flecs::world& m_world;
        const RuntimeComponentRegistry& m_componentRegistry;
        flecs::entity m_sceneTag;
        mutable std::unordered_map<SceneObjectId, flecs::entity_t> m_entitiesById;
        mutable std::unordered_map<std::string, flecs::entity_t> m_entitiesByName;
        std::string m_name;
        std::filesystem::path m_sourcePath;
        std::filesystem::path m_assetRoot;
        std::shared_ptr<AssetService> m_assetService;
        bool m_active = true;
    };
} // namespace Wayfinder
