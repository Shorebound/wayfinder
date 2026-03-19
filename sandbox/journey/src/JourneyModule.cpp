#include "core/Module.h"
#include "core/ModuleRegistry.h"
#include "application/EntryPoint.h"

class JourneyModule : public Wayfinder::Module
{
    void Register(Wayfinder::ModuleRegistry& registry) override
    {
        // Declare game-specific ECS systems here. Each factory is replayed
        // into every new scene world the engine creates.
        //
        // Example:
        // registry.RegisterSystem("MovingPlatforms", [](flecs::world& world) {
        //     world.system<TransformComponent, const MovingPlatformComponent>()
        //         .kind(flecs::OnUpdate)
        //         .each([](TransformComponent& t, const MovingPlatformComponent& mp) {
        //             // ping-pong between waypoints
        //         });
        // });
    }
};

std::unique_ptr<Wayfinder::Module> Wayfinder::CreateModule()
{
    return std::make_unique<JourneyModule>();
}
