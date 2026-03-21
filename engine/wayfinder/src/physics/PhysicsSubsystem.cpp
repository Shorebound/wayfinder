#include "PhysicsSubsystem.h"
#include "../core/Log.h"

namespace Wayfinder
{
    void PhysicsSubsystem::Initialise()
    {
        WAYFINDER_INFO(LogPhysics, "PhysicsSubsystem initialising");
        m_world.Initialise();
    }

    void PhysicsSubsystem::Shutdown()
    {
        WAYFINDER_INFO(LogPhysics, "PhysicsSubsystem shutting down");
        m_world.Shutdown();
    }

} // namespace Wayfinder
