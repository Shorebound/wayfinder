#pragma once

#include <memory>
#include <string>

namespace Wayfinder {

// Forward declarations
class Entity;

class Component {
public:
    Component();
    virtual ~Component();

    // Component lifecycle methods
    virtual void Initialize();
    virtual void Update(float deltaTime);
    virtual void Render();
    virtual void Shutdown();

    // Getters
    bool IsActive() const { return m_isActive; }
    std::weak_ptr<Entity> GetOwner() const { return m_owner; }

    // Setters
    void SetActive(bool active) { m_isActive = active; }
    void SetOwner(std::weak_ptr<Entity> owner) { m_owner = owner; }

private:
    bool m_isActive;
    bool m_isInitialized;
    std::weak_ptr<Entity> m_owner;
};

} // namespace Wayfinder
