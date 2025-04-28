#pragma once

namespace Wayfinder
{
    class Scene;

    class WAYFINDER_API Game
    {
    public:
        Game();
        ~Game();

        virtual bool Initialize();
        virtual void Update(float deltaTime);
        virtual void Shutdown();

        void LoadScene(const std::string& sceneName);
        void UnloadCurrentScene();

        const Scene* GetCurrentScene() const{ return m_currentScene.get(); }

        void SetRunning(bool isRunning)
        {
            m_isRunning = isRunning;
        }
        bool IsRunning() const
        {
            return m_isRunning;
        }

    private:
        std::unique_ptr<Scene> m_currentScene;
        bool m_isRunning;
        bool m_isInitialized;
    };

} // namespace Wayfinder
