#pragma once

#include "../core/Plugin.h"

namespace Wayfinder
{
    /**
     * @brief Registers the Jolt physics subsystem, components, and ECS systems.
     *
     * Add to a game module via:
     * @code
     *   registry.AddPlugin<PhysicsPlugin>();
     * @endcode
     *
     * Registered artefacts:
     * - **PhysicsSubsystem** — GameSubsystem owning the Jolt world.
     * - **RigidBodyComponent** / **ColliderComponent** — serialisable ECS components.
     * - **PhysicsSync** (PreUpdate) — creates Jolt bodies for new entities and syncs kinematic positions.
     * - **PhysicsStep** (OnUpdate) — advances the Jolt simulation.
     * - **PhysicsWriteback** (OnValidate) — copies Jolt positions back to WorldTransformComponent.
     */
    class WAYFINDER_API PhysicsPlugin : public Plugin
    {
    public:
        void Build(ModuleRegistry& registry) override;
    };

} // namespace Wayfinder
