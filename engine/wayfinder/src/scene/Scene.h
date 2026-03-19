#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include <flecs.h>

#include "../core/Identifiers.h"
#include "wayfinder_exports.h"


namespace Wayfinder
{
    class AssetService;
    class Entity;
    class RuntimeComponentRegistry;

    class WAYFINDER_API Scene
    {
    public:
        Scene(flecs::world& world, const RuntimeComponentRegistry& componentRegistry, const std::string& name = "Default Scene");
        ~Scene();

        /// Registers all core ECS components and modules into the given world.
        /// Call once per world before creating any scenes.
        static void RegisterCoreECS(flecs::world& world);

        void Shutdown();

        Entity CreateEntity(const std::string& name = "Entity");
        Entity GetEntityByName(const std::string& name);
        Entity GetEntityById(const SceneObjectId& id);
        bool LoadFromFile(const std::string& filePath);
        bool SaveToFile(const std::string& filePath) const;
        void SetAssetService(const std::shared_ptr<AssetService>& assetService) { m_assetService = assetService; }
        const std::shared_ptr<AssetService>& GetAssetService() const { return m_assetService; }

        const std::string& GetName() const { return m_name; }
        const std::filesystem::path& GetSourcePath() const { return m_sourcePath; }
        const std::filesystem::path& GetAssetRoot() const { return m_assetRoot; }
        
        /// Expose the Flecs world for querying and external registration.
        flecs::world& GetWorld() { return m_world; }
        const flecs::world& GetWorld() const { return m_world; }

    private:
        void ClearEntities();

        flecs::world& m_world;
        const RuntimeComponentRegistry& m_componentRegistry;
        flecs::entity m_sceneTag;
        std::string m_name;
        std::filesystem::path m_sourcePath;
        std::filesystem::path m_assetRoot;
        std::shared_ptr<AssetService> m_assetService;
        bool m_active = true;
    };
} // namespace Wayfinder
