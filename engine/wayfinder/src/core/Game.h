#pragma once

#include <memory>
#include <string>

#include <flecs.h>

#include "wayfinder_exports.h"
#include "../scene/RuntimeComponentRegistry.h"

namespace Wayfinder
{
    class AssetService;
    class ModuleRegistry;
    class Scene;
    struct EngineContext;

    class WAYFINDER_API Game
    {
    public:
        Game();
        ~Game();

        bool Initialize(const EngineContext& ctx);
        void Update(float deltaTime);
        void Shutdown();

        void LoadScene(const std::string& scenePath);
        void UnloadCurrentScene();

        Scene* GetCurrentScene() { return m_currentScene.get(); }
        const Scene* GetCurrentScene() const { return m_currentScene.get(); }
        std::shared_ptr<AssetService> GetAssetService() const { return m_assetService; }

        flecs::world& GetWorld() { return m_world; }
        const flecs::world& GetWorld() const { return m_world; }

        void SetRunning(bool isRunning) { m_running = isRunning; }
        bool IsRunning() const { return m_running; }

    private:
        void InitializeWorld();

        flecs::world m_world;
        RuntimeComponentRegistry m_componentRegistry;
        std::unique_ptr<Scene> m_currentScene;
        std::shared_ptr<AssetService> m_assetService;
        const ModuleRegistry* m_moduleRegistry = nullptr;
        bool m_running = false;
        bool m_initialized = false;
    };

} // namespace Wayfinder
