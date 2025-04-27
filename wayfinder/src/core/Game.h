#pragma once

#include <memory>
#include <string>

namespace Wayfinder
{
    class Scene;

    class IGame
    {
    public:
        virtual bool Initialize() = 0;
        // virtual void PreUpdate() = 0;
        virtual void Update(float deltaTime) = 0;
        // virtual void FixedUpdate() = 0;
        // virtual void PostUpdate() = 0;
        virtual void Shutdown() = 0;
    };

    class Game : public IGame
    {
    public:
        Game();
        ~Game();

        bool Initialize() override;
        void Update(float deltaTime) override;
        void Shutdown() override;

        void LoadScene(const std::string& sceneName);
        void UnloadCurrentScene();

        const Scene* GetCurrentScene() const
        {
            return m_currentScene.get();
        }

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
