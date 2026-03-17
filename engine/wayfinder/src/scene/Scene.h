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

    class WAYFINDER_API Scene
    {
    public:
        Scene(const std::string& name = "Default Scene");
        ~Scene();

        void Initialize();
        void Update(float deltaTime);
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
        
        // Expose the Flecs world for querying
        flecs::world& GetWorld() { return m_world; }
        const flecs::world& GetWorld() const { return m_world; }

    private:
        void ClearEntities();
        void RegisterCoreComponents();
        void RegisterCoreModules();

        std::string m_name;
        std::filesystem::path m_sourcePath;
        std::filesystem::path m_assetRoot;
        std::shared_ptr<AssetService> m_assetService;
        flecs::world m_world; // The main ECS database
        bool m_initialized;
    };
} // namespace Wayfinder
