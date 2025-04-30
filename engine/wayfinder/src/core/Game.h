#pragma once

namespace Wayfinder
{
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
        bool m_running;
        bool m_initialized;
    };

} // namespace Wayfinder
