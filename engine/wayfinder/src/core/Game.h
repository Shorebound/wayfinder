#pragma once

#include <memory>
#include <string>

#include "wayfinder_exports.h"

namespace Wayfinder
{
    class AssetService;
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

        const Scene* GetCurrentScene() const { return m_currentScene.get(); }
        std::shared_ptr<AssetService> GetAssetService() const { return m_assetService; }

        void SetRunning(bool isRunning) { m_running = isRunning; }
        bool IsRunning() const { return m_running; }

    private:
        std::unique_ptr<Scene> m_currentScene;
        std::shared_ptr<AssetService> m_assetService;
        bool m_running = false;
        bool m_initialized = false;
    };

} // namespace Wayfinder
