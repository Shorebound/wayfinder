#include "app/EntryPoint.h"
#include "gameplay/GameState.h"
#include "gameplay/GameplayTag.h"
#include "modules/Module.h"
#include "modules/ModuleExport.h"
#include "modules/ModuleRegistry.h"
#include "modules/Plugin.h"
#include "physics/PhysicsPlugin.h"
#include "scene/entity/Entity.h"

#include "ecs/Flecs.h"

namespace Wayfinder::Journey
{
    struct HealthComponent
    {
        float MaxHealth = 100.0f;
        float CurrentHealth = 100.0f;
    };

    /// Example plugin that registers a custom HealthComponent for scene authoring.
    class HealthPlugin : public Plugin
    {
    public:
        void Build(ModuleRegistry& registry) override
        {
            ModuleRegistry::ComponentDescriptor desc;
            desc.Key = "health";

            desc.RegisterFn = [](flecs::world& world)
            {
                world.component<HealthComponent>();
            };

            desc.ApplyFn = [](const nlohmann::json& table, Entity& entity)
            {
                HealthComponent health;
                health.MaxHealth = table.value("max_health", 100.0f);
                health.CurrentHealth = table.value("current_health", health.MaxHealth);
                entity.AddComponent<HealthComponent>(health);
            };

            desc.SerialiseFn = [](const Entity& entity, nlohmann::json& tables)
            {
                if (!entity.HasComponent<HealthComponent>())
                {
                    return;
                }

                const auto& health = entity.GetComponent<HealthComponent>();
                nlohmann::json t;
                t["max_health"] = health.MaxHealth;
                t["current_health"] = health.CurrentHealth;
                tables["health"] = std::move(t);
            };

            desc.ValidateFn = [](const nlohmann::json& table, std::string& error) -> bool
            {
                if (table.contains("max_health") && !table["max_health"].is_number())
                {
                    error = "'max_health' must be a number";
                    return false;
                }

                if (table.contains("current_health") && !table["current_health"].is_number())
                {
                    error = "'current_health' must be a number";
                    return false;
                }

                return true;
            };

            registry.RegisterComponent(std::move(desc));
        }
    };

    /// Example plugin that registers game states and conditioned systems.
    class GameplayPlugin : public Plugin
    {
    public:
        void Build(ModuleRegistry& registry) override
        {
            // Register game states with lifecycle callbacks
            registry.RegisterState({"Playing", nullptr, nullptr});
            registry.RegisterState({"Paused", nullptr, nullptr});

            // A system that only runs while in the "Playing" state.
            // Convention: the flecs system name must match the descriptor name
            // so the engine can look it up and toggle it via enable/disable.
            // Runs after BurnDamage so regen applies after damage each frame.
            registry.RegisterSystem("HealthRegen",
                [](flecs::world& world)
                {
                    world.system<HealthComponent>("HealthRegen")
                        .kind(flecs::OnUpdate)
                        .each([](HealthComponent& health)
                            {
                                if (health.CurrentHealth < health.MaxHealth)
                                {
                                    health.CurrentHealth += 0.1f;
                                    if (health.CurrentHealth > health.MaxHealth)
                                    {
                                        health.CurrentHealth = health.MaxHealth;
                                    }
                                }
                            });
                },
                Wayfinder::InState("Playing"), {"BurnDamage"});

            registry.SetInitialState("Playing");
        }
    };

    /// Example plugin demonstrating gameplay tag registration.
    class TagDemoPlugin : public Plugin
    {
    public:
        void Build(ModuleRegistry& registry) override
        {
            // Load data-driven tag files from config/tags/
            registry.RegisterTagFile("tags/status.tags.toml");
            registry.RegisterTagFile("tags/faction.tags.toml");

            // Code-defined tags with comments — capture the returned tag for use
            registry.RegisterTag("Status.Stunned", "Entity is stunned and cannot act");
            auto burning = registry.RegisterTag("Status.Burning", "Entity is on fire");

            // A system that runs when the "Status.Burning" tag is active at the
            // world level (via ActiveGameplayTags).  This is intentionally global:
            // when the tag is set, ALL entities with HealthComponent take burn
            // damage.  For per-entity burning, attach a GameplayTagContainer to
            // each entity and query it inside the lambda instead.
            registry.RegisterSystem(
                "BurnDamage",
                [](flecs::world& world)
                {
                    world.system<HealthComponent>("BurnDamage")
                        .kind(flecs::OnUpdate)
                        .each([](HealthComponent& health)
                            {
                                if (health.CurrentHealth > 0.0f)
                                {
                                    health.CurrentHealth -= 0.5f;
                                    if (health.CurrentHealth < 0.0f)
                                    {
                                        health.CurrentHealth = 0.0f;
                                    }
                                }
                            });
                },
                HasTag(burning));
        }
    };

    class JourneyModule : public Module
    {
        void Register(ModuleRegistry& registry) override
        {
            registry.AddPlugin<Physics::PhysicsPlugin>();
            registry.AddPlugin<HealthPlugin>();
            registry.AddPlugin<GameplayPlugin>();
            registry.AddPlugin<TagDemoPlugin>();
        }
    };
} // namespace Wayfinder::Journey

std::unique_ptr<Wayfinder::Module> Wayfinder::CreateModule()
{
    return std::make_unique<Journey::JourneyModule>();
}

// Dynamic entry point for tools loading the module as a shared library.
WAYFINDER_IMPLEMENT_MODULE(Wayfinder::Journey::JourneyModule)
