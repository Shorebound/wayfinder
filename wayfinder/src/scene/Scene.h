#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace Wayfinder
{

    class Entity;

    class Scene
    {
    public:
        Scene(const std::string &name = "Default Scene");
        ~Scene();

        void Initialize();
        void Update(float deltaTime);
        void Render();
        void Shutdown();

        std::shared_ptr<Entity> CreateEntity(const std::string &name = "Entity");
        void AddEntity(std::shared_ptr<Entity> entity);
        void RemoveEntity(std::shared_ptr<Entity> entity);
        void RemoveEntityByID(uint64_t entityID);

        std::shared_ptr<Entity> GetEntityByID(uint64_t entityID) const;
        std::shared_ptr<Entity> GetEntityByName(const std::string &name) const;
        std::vector<std::shared_ptr<Entity>> GetAllEntities() const;

        const std::string &GetName() const { return m_name; }
        size_t GetEntityCount() const { return m_entities.size(); }

    private:
        std::string m_name;
        std::unordered_map<uint64_t, std::shared_ptr<Entity>> m_entities;
        bool m_isInitialized;
    };
} // namespace Wayfinder
