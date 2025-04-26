#pragma once

#include <vector>
#include <memory>
#include <string>

namespace Wayfinder {

// Forward declarations
class Entity; // We'll implement this later if needed

class Scene {
public:
    Scene(const std::string& name = "Default Scene");
    ~Scene();

    // Scene lifecycle methods
    void Initialize();
    void Update(float deltaTime);
    void Shutdown();

    // Entity management
    // Note: These are placeholders - we'll implement proper entity management later
    void AddEntity(std::shared_ptr<Entity> entity);
    void RemoveEntity(std::shared_ptr<Entity> entity);
    
    // Getters
    const std::string& GetName() const { return m_name; }
    
private:
    std::string m_name;
    std::vector<std::shared_ptr<Entity>> m_entities; // Placeholder for entity storage
    bool m_isInitialized;
};

} // namespace Wayfinder
