#include "app/Application.h"
#include "gameplay/GameState.h"
#include "gameplay/Tag.h"
#include "physics/PhysicsPlugin.h"
#include "plugins/Plugin.h"
#include "plugins/PluginRegistry.h"
#include "scene/SceneWorldBootstrap.h"
#include "scene/entity/Entity.h"

#include "ecs/Flecs.h"

#include <nlohmann/json.hpp>

namespace
{
    struct HealthComponent
    {
        float MaxHealth = 100.0f;
        float CurrentHealth = 100.0f;
    };

    /// Example plugin that registers a custom HealthComponent for scene authoring.
    class HealthPlugin : public Wayfinder::Plugins::Plugin
    {
    public:
        void Build(Wayfinder::Plugins::PluginRegistry& registry) override
        {
            Wayfinder::Plugins::PluginRegistry::ComponentDescriptor desc;
            desc.Key = "health";

            desc.RegisterFn = [](flecs::world& world)
            {
                world.component<HealthComponent>();
            };

            desc.ApplyFn = [](const nlohmann::json& table, Wayfinder::Entity& entity)
            {
                HealthComponent health;
                health.MaxHealth = table.value("max_health", 100.0f);
                health.CurrentHealth = table.value("current_health", health.MaxHealth);
                entity.AddComponent<HealthComponent>(health);
            };

            desc.SerialiseFn = [](const Wayfinder::Entity& entity, nlohmann::json& tables)
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

                if (table.contains("max_health") && table["max_health"].is_number())
                {
                    const float maxHealth = table["max_health"].get<float>();
                    if (maxHealth < 0.0f)
                    {
                        error = "'max_health' must be non-negative";
                        return false;
                    }
                }

                if (table.contains("current_health") && table["current_health"].is_number())
                {
                    const float currentHealth = table["current_health"].get<float>();
                    if (currentHealth < 0.0f)
                    {
                        error = "'current_health' must be non-negative";
                        return false;
                    }
                }

                if (table.contains("max_health") && table["max_health"].is_number() && table.contains("current_health") && table["current_health"].is_number())
                {
                    const float maxHealth = table["max_health"].get<float>();
                    const float currentHealth = table["current_health"].get<float>();
                    if (currentHealth > maxHealth)
                    {
                        error = "'current_health' cannot exceed 'max_health'";
                        return false;
                    }
                }

                return true;
            };

            registry.RegisterComponent(std::move(desc));
        }
    };

    /// Example plugin that registers game states and conditioned systems.
    class GameplayPlugin : public Wayfinder::Plugins::Plugin
    {
    public:
        void Build(Wayfinder::Plugins::PluginRegistry& registry) override
        {
            // Register game states with lifecycle callbacks
            registry.RegisterState({.Name = "Playing", .OnEnter = nullptr, .OnExit = nullptr});
            registry.RegisterState({.Name = "Paused", .OnEnter = nullptr, .OnExit = nullptr});

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
                        health.CurrentHealth = std::min(health.CurrentHealth + 0.1f, health.MaxHealth);
                    }
                });
            }, Wayfinder::InState("Playing"), {"BurnDamage"});

            registry.SetInitialState("Playing");
        }
    };

    /// Example plugin demonstrating gameplay tag registration.
    class TagDemoPlugin : public Wayfinder::Plugins::Plugin
    {
    public:
        void Build(Wayfinder::Plugins::PluginRegistry& registry) override
        {
            // Load data-driven tag files from config/tags/
            registry.RegisterTagFile("tags/status.tags.toml");
            registry.RegisterTagFile("tags/faction.tags.toml");

            // Code-defined tags with comments - capture the returned tag for use
            registry.RegisterTag("Status.Stunned", "Entity is stunned and cannot act");
            auto burning = registry.RegisterTag("Status.Burning", "Entity is on fire");

            // A system that runs when the "Status.Burning" tag is active at the
            // world level (via ActiveTags).  This is intentionally global:
            // when the tag is set, ALL entities with HealthComponent take burn
            // each entity and query it inside the lambda instead.
            registry.RegisterSystem("BurnDamage", [](flecs::world& world)
            {
                world.system<HealthComponent>("BurnDamage")
                    .kind(flecs::OnUpdate)
                    .each([](HealthComponent& health)
                {
                    if (health.CurrentHealth > 0.0f)
                    {
                        health.CurrentHealth = std::max(health.CurrentHealth - 0.5f, 0.0f);
                    }
                });
            }, Wayfinder::HasTag(burning));
        }
    };

    class JourneyGame : public Wayfinder::Plugins::Plugin
    {
    public:
        void Build(Wayfinder::Plugins::PluginRegistry& registry) override
        {
            Wayfinder::PopulateDefaultScenePlugins(registry);
            registry.AddPlugin<Wayfinder::Physics::PhysicsPlugin>();
            registry.AddPlugin<HealthPlugin>();
            registry.AddPlugin<GameplayPlugin>();
            registry.AddPlugin<TagDemoPlugin>();
        }
    };
} // namespace

int main(int argc, char* argv[])
{
    auto gamePlugin = std::make_unique<JourneyGame>();
    const Wayfinder::Application::CommandLineArgs cmdArgs{.Count = argc, .Args = argv};
    Wayfinder::Application app(std::move(gamePlugin), cmdArgs);
    app.Run();
    return 0;
}
