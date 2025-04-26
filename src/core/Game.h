#pragma once

#include <memory>
#include <string>

namespace Wayfinder {

// Forward declarations
class Scene;

class Game {
public:
    Game();
    ~Game();

    // Game lifecycle methods
    bool Initialize();
    void Update(float deltaTime);
    void Shutdown();

    // Scene management
    void LoadScene(const std::string& sceneName);
    void UnloadCurrentScene();
    
    // Getters
    std::weak_ptr<Scene> GetCurrentScene() const { return m_currentScene; }
    bool IsRunning() const { return m_isRunning; }
    
    // Setters
    void SetRunning(bool isRunning) { m_isRunning = isRunning; }

private:
    std::shared_ptr<Scene> m_currentScene;
    bool m_isRunning;
    bool m_isInitialized;
};

} // namespace Wayfinder
