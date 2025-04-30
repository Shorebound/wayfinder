#pragma once
#include "Component.h"


namespace Wayfinder
{

    class Transform;
    
    class WAYFINDER_API Entity : public std::enable_shared_from_this<Entity>
    {
    public:
        Entity(const std::string& name = "Entity");
        virtual ~Entity();

        virtual void Initialize();
        virtual void Update(float deltaTime);
        virtual void Render();
        virtual void Shutdown();

        template <typename T, typename... Args>
        std::unique_ptr<T> AddComponent(Args&&... args);

        template <typename T>
        std::unique_ptr<T> GetComponent() const;

        template <typename T>
        bool HasComponent() const;

        template <typename T>
        void RemoveComponent();

        const std::string& GetName() const { return m_name; }
        uint64_t GetID() const { return m_id; }
        bool IsActive() const { return m_active; }
        const Transform* GetTransform() const;

        void SetName(const std::string& name) { m_name = name; }
        void SetActive(bool active) { m_active = active; }

    private:
        std::string m_name;
        uint64_t m_id;
        bool m_active;
        bool m_initialized;

        std::unordered_map<std::type_index, std::shared_ptr<Component>> m_components;

        std::unique_ptr<Transform> m_transform;

        static uint64_t s_nextEntityID;
    };

    // Template implementations
    template <typename T, typename... Args>
    std::unique_ptr<T> Entity::AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

        const auto typeIndex = std::type_index(typeid(T));
        if (m_components.contains(typeIndex))
        {
            return std::dynamic_pointer_cast<T>(m_components[typeIndex]);
        }

        std::unique_ptr<T> component = std::make_unique<T>(std::forward<Args>(args)...);
        m_components[typeIndex] = component;

        component->SetOwner(shared_from_this());

        if (m_initialized)
        {
            component->Initialize();
        }

        return component;
    }

    template <typename T>
    std::unique_ptr<T> Entity::GetComponent() const
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

        const auto typeIndex = std::type_index(typeid(T));
        if (const auto it = m_components.find(typeIndex); it != m_components.end())
        {
            return std::dynamic_pointer_cast<T>(it->second);
        }

        return nullptr;
    }

    template <typename T>
    bool Entity::HasComponent() const
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

        const auto typeIndex = std::type_index(typeid(T));
        return m_components.contains(typeIndex);
    }

    template <typename T>
    void Entity::RemoveComponent()
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

        const auto typeIndex = std::type_index(typeid(T));
        if (const auto it = m_components.find(typeIndex); it != m_components.end())
        {
            it->second->Shutdown();
            m_components.erase(it);
        }
    }

} // namespace Wayfinder

