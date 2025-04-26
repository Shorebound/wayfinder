#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>

namespace Wayfinder {

// Forward declarations
class Component;
class Transform;

class Entity : public std::enable_shared_from_this<Entity> {
public:
    Entity(const std::string& name = "Entity");
    virtual ~Entity();

    // Entity lifecycle methods
    virtual void Initialize();
    virtual void Update(float deltaTime);
    virtual void Render();
    virtual void Shutdown();

    // Component management
    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args);

    template<typename T>
    std::shared_ptr<T> GetComponent() const;

    template<typename T>
    bool HasComponent() const;

    template<typename T>
    void RemoveComponent();

    // Getters
    const std::string& GetName() const { return m_name; }
    uint64_t GetID() const { return m_id; }
    bool IsActive() const { return m_isActive; }
    std::shared_ptr<Transform> GetTransform() const;

    // Setters
    void SetName(const std::string& name) { m_name = name; }
    void SetActive(bool active) { m_isActive = active; }

private:
    std::string m_name;
    uint64_t m_id;
    bool m_isActive;
    bool m_isInitialized;

    // Component storage
    std::unordered_map<std::type_index, std::shared_ptr<Component>> m_components;
    
    // Transform component (special case - all entities have a transform)
    std::shared_ptr<Transform> m_transform;

    // ID generation
    static uint64_t s_nextEntityID;
};

// Template implementations
template<typename T, typename... Args>
std::shared_ptr<T> Entity::AddComponent(Args&&... args) {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
    
    // Check if component of this type already exists
    auto typeIndex = std::type_index(typeid(T));
    if (m_components.find(typeIndex) != m_components.end()) {
        return std::dynamic_pointer_cast<T>(m_components[typeIndex]);
    }
    
    // Create new component
    std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
    m_components[typeIndex] = component;
    
    // Set component's owner
    component->SetOwner(shared_from_this());
    
    // Initialize component if entity is already initialized
    if (m_isInitialized) {
        component->Initialize();
    }
    
    return component;
}

template<typename T>
std::shared_ptr<T> Entity::GetComponent() const {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
    
    auto typeIndex = std::type_index(typeid(T));
    auto it = m_components.find(typeIndex);
    if (it != m_components.end()) {
        return std::dynamic_pointer_cast<T>(it->second);
    }
    
    return nullptr;
}

template<typename T>
bool Entity::HasComponent() const {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
    
    auto typeIndex = std::type_index(typeid(T));
    return m_components.find(typeIndex) != m_components.end();
}

template<typename T>
void Entity::RemoveComponent() {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
    
    auto typeIndex = std::type_index(typeid(T));
    auto it = m_components.find(typeIndex);
    if (it != m_components.end()) {
        it->second->Shutdown();
        m_components.erase(it);
    }
}

} // namespace Wayfinder
