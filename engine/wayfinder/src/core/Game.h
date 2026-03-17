#pragma once

namespace Wayfinder
{
    class AssetService;
    class Scene;

    class WAYFINDER_API Game
    {
    public:
        Game();
        virtual ~Game();

        virtual bool Initialize();
        virtual void Update(float deltaTime);
        virtual void Shutdown();

        void LoadScene(const std::string& sceneName);
        void UnloadCurrentScene();

        const Scene* GetCurrentScene() const{ return m_currentScene.get(); }
        std::shared_ptr<AssetService> GetAssetService() const { return m_assetService; }

        void SetRunning(bool isRunning)
        {
            m_running = isRunning;
        }
        bool IsRunning() const
        {
            return m_running;
        }

    private:
        std::unique_ptr<Scene> m_currentScene;
        std::shared_ptr<AssetService> m_assetService;
        bool m_running;
        bool m_initialized;
    };

} // namespace Wayfinder
