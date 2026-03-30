#include "PhysicsSubsystem.h"
#include "core/Log.h"

namespace Wayfinder::Physics
{
    void PhysicsSubsystem::Initialise()
    {
        Log::Info(LogPhysics, "PhysicsSubsystem initialising");
        m_world.Initialise();
    }

    void PhysicsSubsystem::Shutdown()
    {
        Log::Info(LogPhysics, "PhysicsSubsystem shutting down");
        m_world.Shutdown();
    }

} // namespace Wayfinder::Physics
