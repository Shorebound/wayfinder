#pragma once

#include "PhysicsWorld.h"
#include "app/Subsystem.h"

namespace Wayfinder::Physics
{
    /**
     * @brief Game-scoped subsystem that owns the Jolt physics world.
     *
     * Registered via PhysicsPlugin and created automatically by the
     * SubsystemCollection during Game initialisation.
     *
     * Access from any ECS system or game code:
     * @code
     *   auto& physics = GameSubsystems::Get<PhysicsSubsystem>();
     *   physics.GetWorld().Step(dt);
     * @endcode
     */
    class WAYFINDER_API PhysicsSubsystem : public GameSubsystem
    {
    public:
        void Initialise() override;
        void Shutdown() override;

        PhysicsWorld& GetWorld() { return m_world; }
        const PhysicsWorld& GetWorld() const { return m_world; }

    private:
        PhysicsWorld m_world;
    };

} // namespace Wayfinder::Physics
