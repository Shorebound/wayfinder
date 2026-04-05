#pragma once

namespace Wayfinder
{
    class EngineContext;

    /**
     * @brief Flecs singleton component providing access to the engine context.
     *
     * Set on state entry, removed on state exit. Replaces the static
     * GameSubsystems accessor with a thread-safe ECS singleton pattern.
     * Thread-safe by design: flecs controls singleton access through its
     * staging model.
     *
     * @code
     * // On state entry:
     * world.set<EngineContextRef>({.Context = &context});
     *
     * // In flecs systems:
     * auto* ref = world.get<EngineContextRef>();
     * auto& physics = ref->Context->GetStateSubsystem<PhysicsSubsystem>();
     *
     * // On state exit:
     * world.remove<EngineContextRef>();
     * @endcode
     */
    struct EngineContextRef
    {
        EngineContext* Context = nullptr;
    };

} // namespace Wayfinder
