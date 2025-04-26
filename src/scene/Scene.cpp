#include "Scene.h"
#include "raylib.h"

namespace Wayfinder {

Scene::Scene(const std::string& name)
    : m_name(name)
    , m_isInitialized(false)
{
}

Scene::~Scene()
{
    if (m_isInitialized)
    {
        Shutdown();
    }
}

void Scene::Initialize()
{
    // Initialize the scene
    TraceLog(LOG_INFO, "Initializing scene: %s", m_name.c_str());
    
    // Initialize all entities
    for (auto& entity : m_entities)
    {
        // We'll implement entity initialization later
    }
    
    m_isInitialized = true;
}

void Scene::Update(float deltaTime)
{
    // Update all entities in the scene
    for (auto& entity : m_entities)
    {
        // We'll implement entity updating later
    }
}

void Scene::Shutdown()
{
    // Clean up scene resources
    TraceLog(LOG_INFO, "Shutting down scene: %s", m_name.c_str());
    
    // Clear all entities
    m_entities.clear();
    
    m_isInitialized = false;
}

void Scene::AddEntity(std::shared_ptr<Entity> entity)
{
    // Add entity to the scene
    m_entities.push_back(entity);
}

void Scene::RemoveEntity(std::shared_ptr<Entity> entity)
{
    // Remove entity from the scene
    // This is a simple implementation - we'll improve it later
    for (auto it = m_entities.begin(); it != m_entities.end(); ++it)
    {
        if (*it == entity)
        {
            m_entities.erase(it);
            break;
        }
    }
}

} // namespace Wayfinder
