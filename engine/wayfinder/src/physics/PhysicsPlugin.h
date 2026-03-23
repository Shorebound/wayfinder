#pragma once

#include "plugins/Plugin.h"

namespace Wayfinder::Physics
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
     * - **PhysicsCreateBodies** (OnSet observer) — creates Jolt bodies reactively when
     *   an entity gains RigidBodyComponent + ColliderComponent + TransformComponent.
     * - **PhysicsDestroyBodies** (OnRemove observer) — destroys Jolt bodies automatically
     *   when RigidBodyComponent is removed or the entity is deleted.
     * - **PhysicsStep** (OnUpdate) — advances the Jolt simulation.
     * - **PhysicsSyncTransforms** (OnValidate) — copies Jolt position and rotation
     *   back into WorldTransformComponent with full LocalToWorld matrix rebuild.
     */
    class WAYFINDER_API PhysicsPlugin : public Plugin
    {
    public:
        void Build(PluginRegistry& registry) override;
    };

} // namespace Wayfinder::Physics
