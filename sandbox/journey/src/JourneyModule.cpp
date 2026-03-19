#include "core/Module.h"
#include "core/ModuleRegistry.h"
#include "core/GameplayTag.h"
#include "core/GameState.h"
#include "core/Plugin.h"
#include "scene/entity/Entity.h"
#include "application/EntryPoint.h"

#include <flecs.h>
#include <toml++/toml.hpp>

namespace
{
    struct HealthComponent
    {
        float MaxHealth = 100.0f;
        float CurrentHealth = 100.0f;
    };
}

/// Example plugin that registers a custom HealthComponent for scene authoring.
class HealthPlugin : public Wayfinder::Plugin
{
public:
    void Build(Wayfinder::ModuleRegistry& registry) override
    {
        Wayfinder::ModuleRegistry::ComponentDescriptor desc;
        desc.Key = "health";

        desc.RegisterFn = [](flecs::world& world)
        {
            world.component<HealthComponent>();
        };

        desc.ApplyFn = [](const toml::table& table, Wayfinder::Entity& entity)
        {
            HealthComponent health;
            health.MaxHealth = table["max_health"].value_or(100.0f);
            health.CurrentHealth = table["current_health"].value_or(health.MaxHealth);
            entity.AddComponent<HealthComponent>(health);
        };

        desc.SerializeFn = [](const Wayfinder::Entity& entity, toml::table& tables)
        {
            if (!entity.HasComponent<HealthComponent>())
                return;

            const auto& health = entity.GetComponent<HealthComponent>();
            toml::table t;
            t.insert("max_health", health.MaxHealth);
            t.insert("current_health", health.CurrentHealth);
            tables.insert("health", std::move(t));
        };

        desc.ValidateFn = [](const toml::table& table, std::string& error) -> bool
        {
            if (const auto* node = table.get("max_health"); node && !node->is_floating_point() && !node->is_integer())
            {
                error = "'max_health' must be a number";
                return false;
            }

            if (const auto* node = table.get("current_health"); node && !node->is_floating_point() && !node->is_integer())
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
class GameplayPlugin : public Wayfinder::Plugin
{
public:
    void Build(Wayfinder::ModuleRegistry& registry) override
    {
        // Register game states with lifecycle callbacks
        registry.RegisterState({"Playing", nullptr, nullptr});
        registry.RegisterState({"Paused", nullptr, nullptr});

        // A system that only runs while in the "Playing" state.
        // Convention: the flecs system name must match the descriptor name
        // so the engine can look it up and toggle it via enable/disable.
        // Runs after BurnDamage so regen applies after damage each frame.
        registry.RegisterSystem("HealthRegen", [](flecs::world& world)
        {
            world.system<HealthComponent>("HealthRegen")
                .kind(flecs::OnUpdate)
                .each([](HealthComponent& health)
                {
                    if (health.CurrentHealth < health.MaxHealth)
                    {
                        health.CurrentHealth += 0.1f;
                        if (health.CurrentHealth > health.MaxHealth)
                            health.CurrentHealth = health.MaxHealth;
                    }
                });
        }, Wayfinder::InState("Playing"), {"BurnDamage"});

        registry.SetInitialState("Playing");
    }
};

/// Example plugin demonstrating gameplay tag registration.
class TagDemoPlugin : public Wayfinder::Plugin
{
public:
    void Build(Wayfinder::ModuleRegistry& registry) override
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
        registry.RegisterSystem("BurnDamage", [](flecs::world& world)
        {
            world.system<HealthComponent>("BurnDamage")
                .kind(flecs::OnUpdate)
                .each([](HealthComponent& health)
                {
                    if (health.CurrentHealth > 0.0f)
                    {
                        health.CurrentHealth -= 0.5f;
                        if (health.CurrentHealth < 0.0f)
                            health.CurrentHealth = 0.0f;
                    }
                });
        }, Wayfinder::HasTag(burning));
    }
};

class JourneyModule : public Wayfinder::Module
{
    void Register(Wayfinder::ModuleRegistry& registry) override
    {
        registry.AddPlugin<HealthPlugin>();
        registry.AddPlugin<GameplayPlugin>();
        registry.AddPlugin<TagDemoPlugin>();
    }
};

std::unique_ptr<Wayfinder::Module> Wayfinder::CreateModule()
{
    return std::make_unique<JourneyModule>();
}
