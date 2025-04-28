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

        bool IsActive() const { return m_isActive; }
        std::weak_ptr<Entity> GetOwner() const { return m_owner; }

        void SetActive(bool active) { m_isActive = active; }
        void SetOwner(std::weak_ptr<Entity> owner) { m_owner = owner; }

    private:
        bool m_isActive;
        bool m_isInitialized;
        std::weak_ptr<Entity> m_owner;
    };

} // namespace Wayfinder
