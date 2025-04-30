#include <utility>

#pragma once


namespace Wayfinder
{
    class Entity;

    class WAYFINDER_API Component
    {
    public:
        Component();
        virtual ~Component();

        virtual void Initialize();
        virtual void Update(float deltaTime);
        virtual void Render();
        virtual void Shutdown();

        bool IsActive() const { return m_active; }
        std::weak_ptr<Entity> GetOwner() const { return m_owner; }

        void SetActive(bool active) { m_active = active; }
        void SetOwner(std::weak_ptr<Entity> owner) { m_owner = std::move(owner); }

    private:
        bool m_active;
        bool m_initialized;
        std::weak_ptr<Entity> m_owner;
    };

} // namespace Wayfinder
