#include "Entity.h"
#include "Component.h"
#include "Transform.h"
#include "raylib.h"

namespace Wayfinder {

// Initialize static ID counter
uint64_t Entity::s_nextEntityID = 1;

Entity::Entity(const std::string& name)
    : m_name(name)
    , m_id(s_nextEntityID++)
    , m_isActive(true)
    , m_isInitialized(false)
{
    // Every entity has a transform component by default
    m_transform = std::make_shared<Transform>();
}

Entity::~Entity()
{
    if (m_isInitialized)
    {
        Shutdown();
    }
}

void Entity::Initialize()
{
    if (m_isInitialized) return;
    
    TraceLog(LOG_INFO, "Initializing entity: %s (ID: %llu)", m_name.c_str(), m_id);
    
    // Initialize transform first
    m_transform->SetOwner(shared_from_this());
    m_transform->Initialize();
    
    // Initialize all components
    for (auto& pair : m_components)
    {
        pair.second->Initialize();
    }
    
    m_isInitialized = true;
}

void Entity::Update(float deltaTime)
{
    if (!m_isActive) return;
    
    // Update transform first
    m_transform->Update(deltaTime);
    
    // Update all components
    for (auto& pair : m_components)
    {
        if (pair.second->IsActive())
        {
            pair.second->Update(deltaTime);
        }
    }
}

void Entity::Render()
{
    if (!m_isActive) return;
    
    // Render all components
    for (auto& pair : m_components)
    {
        if (pair.second->IsActive())
        {
            pair.second->Render();
        }
    }
}

void Entity::Shutdown()
{
    TraceLog(LOG_INFO, "Shutting down entity: %s (ID: %llu)", m_name.c_str(), m_id);
    
    // Shutdown all components
    for (auto& pair : m_components)
    {
        pair.second->Shutdown();
    }
    
    // Clear components
    m_components.clear();
    
    // Shutdown transform
    if (m_transform)
    {
        m_transform->Shutdown();
        m_transform = nullptr;
    }
    
    m_isInitialized = false;
}

std::shared_ptr<Transform> Entity::GetTransform() const
{
    return m_transform;
}

} // namespace Wayfinder
