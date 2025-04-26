#include "Scene.h"
#include "../entity/Entity.h"
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
    for (auto& pair : m_entities)
    {
        pair.second->Initialize();
    }

    m_isInitialized = true;
}

void Scene::Update(float deltaTime)
{
    // Update all entities in the scene
    for (auto& pair : m_entities)
    {
        if (pair.second->IsActive())
        {
            pair.second->Update(deltaTime);
        }
    }
}

void Scene::Render()
{
    // Render all entities in the scene
    for (auto& pair : m_entities)
    {
        if (pair.second->IsActive())
        {
            pair.second->Render();
        }
    }
}

void Scene::Shutdown()
{
    // Clean up scene resources
    TraceLog(LOG_INFO, "Shutting down scene: %s", m_name.c_str());

    // Shutdown all entities
    for (auto& pair : m_entities)
    {
        pair.second->Shutdown();
    }

    // Clear all entities
    m_entities.clear();

    m_isInitialized = false;
}

std::shared_ptr<Entity> Scene::CreateEntity(const std::string& name)
{
    std::shared_ptr<Entity> entity = std::make_shared<Entity>(name);
    AddEntity(entity);

    if (m_isInitialized)
    {
        entity->Initialize();
    }

    return entity;
}

void Scene::AddEntity(std::shared_ptr<Entity> entity)
{
    // Add entity to the scene
    uint64_t entityID = entity->GetID();
    m_entities[entityID] = entity;

    // Initialize the entity if the scene is already initialized
    if (m_isInitialized && !entity->IsActive())
    {
        entity->Initialize();
    }
}

void Scene::RemoveEntity(std::shared_ptr<Entity> entity)
{
    if (entity)
    {
        RemoveEntityByID(entity->GetID());
    }
}

void Scene::RemoveEntityByID(uint64_t entityID)
{
    auto it = m_entities.find(entityID);
    if (it != m_entities.end())
    {
        it->second->Shutdown();
        m_entities.erase(it);
    }
}

std::shared_ptr<Entity> Scene::GetEntityByID(uint64_t entityID) const
{
    auto it = m_entities.find(entityID);
    if (it != m_entities.end())
    {
        return it->second;
    }

    return nullptr;
}

std::shared_ptr<Entity> Scene::GetEntityByName(const std::string& name) const
{
    for (auto& pair : m_entities)
    {
        if (pair.second->GetName() == name)
        {
            return pair.second;
        }
    }

    return nullptr;
}

std::vector<std::shared_ptr<Entity>> Scene::GetAllEntities() const
{
    std::vector<std::shared_ptr<Entity>> entities;
    entities.reserve(m_entities.size());

    for (auto& pair : m_entities)
    {
        entities.push_back(pair.second);
    }

    return entities;
}

} // namespace Wayfinder
