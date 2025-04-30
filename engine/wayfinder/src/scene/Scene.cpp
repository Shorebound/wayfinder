#include "Scene.h"
#include "entity/Entity.h"
#include "../core/Log.h"

#include <ranges>

namespace Wayfinder
{

    Scene::Scene(const std::string& name) : m_name(name), m_initialized(false)
    {
    }

    Scene::~Scene()
    {
        if (m_initialized)
        {
            Shutdown();
        }
    }

    void Scene::Initialize()
    {
        WAYFINDER_INFO(LogScene, "Initializing scene: {0}", m_name);

        for (const auto& entity : m_entities | std::views::values)
        {
            entity->Initialize();
        }

        m_initialized = true;
    }

    void Scene::Update(float deltaTime)
    {
        for (const auto& entity : m_entities | std::views::values)
        {
            if (entity->IsActive())
            {
                entity->Update(deltaTime);
            }
        }
    }

    void Scene::Render()
    {
        for (const auto& entity : m_entities | std::views::values)
        {
            if (entity->IsActive())
            {
                entity->Render();
            }
        }
    }

    void Scene::Shutdown()
    {
        WAYFINDER_INFO(LogScene, "Shutting down scene: {0}", m_name);

        for (const auto& entity : m_entities | std::views::values)
        {
            entity->Shutdown();
        }

        m_entities.clear();

        m_initialized = false;
    }

    std::shared_ptr<Entity> Scene::CreateEntity(const std::string& name)
    {
        auto entity = std::make_shared<Entity>(name);
        AddEntity(entity);

        if (m_initialized)
        {
            entity->Initialize();
        }

        WAYFINDER_INFO(LogScene, "Created entity: {0} (ID: {1})", name, entity->GetID());
        return entity;
    }

    void Scene::AddEntity(const std::shared_ptr<Entity>& entity)
    {
        const uint64_t entityID = entity->GetID();
        m_entities[entityID] = entity;

        if (m_initialized && !entity->IsActive())
        {
            entity->Initialize();
        }
    }

    void Scene::RemoveEntity(const std::shared_ptr<Entity>& entity)
    {
        if (entity)
        {
            RemoveEntityByID(entity->GetID());
        }
    }

    void Scene::RemoveEntityByID(const uint64_t entityID)
    {
        if (const auto it = m_entities.find(entityID); it != m_entities.end())
        {
            it->second->Shutdown();
            m_entities.erase(it);
        }
    }

    std::shared_ptr<Entity> Scene::GetEntityByID(const uint64_t entityID) const
    {
        if (const auto it = m_entities.find(entityID); it != m_entities.end())
        {
            return it->second;
        }

        return nullptr;
    }

    std::shared_ptr<Entity> Scene::GetEntityByName(const std::string& name) const
    {
        for (const auto& entity : m_entities | std::views::values)
        {
            if (entity->GetName() == name)
            {
                return entity;
            }
        }

        return nullptr;
    }

    std::vector<std::shared_ptr<Entity>> Scene::GetAllEntities() const
    {
        std::vector<std::shared_ptr<Entity>> entities;
        entities.reserve(m_entities.size());

        for (const auto& entity : m_entities | std::views::values)
        {
            entities.push_back(entity);
        }

        return entities;
    }

} // namespace Wayfinder
