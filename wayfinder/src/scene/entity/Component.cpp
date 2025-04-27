#include "Component.h"
#include "Entity.h"
#include "raylib.h"

namespace Wayfinder
{

    Component::Component()
        : m_isActive(true), m_isInitialized(false)
    {
    }

    Component::~Component()
    {
        if (m_isInitialized)
        {
            Shutdown();
        }
    }

    void Component::Initialize()
    {
        if (m_isInitialized)
            return;

        m_isInitialized = true;
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
        m_isInitialized = false;
    }

} // namespace Wayfinder
