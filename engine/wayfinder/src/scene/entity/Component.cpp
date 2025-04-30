#include "Component.h"
#include "Entity.h"

namespace Wayfinder
{

    Component::Component()
        : m_active(true), m_initialized(false)
    {
    }

    Component::~Component()
    {
        if (m_initialized)
        {
            Shutdown();
        }
    }

    void Component::Initialize()
    {
        if (m_initialized)
            return;

        m_initialized = true;
    }

    void Component::Update(float deltaTime)
    {
        // Base implementation does nothing
    }

    void Component::Render()
    {
        // Base implementation does nothing
    }

    void Component::Shutdown()
    {
        m_initialized = false;
    }

} // namespace Wayfinder
