#include "core/Module.h"
#include "core/ModuleRegistry.h"
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

class JourneyModule : public Wayfinder::Module
{
    void Register(Wayfinder::ModuleRegistry& registry) override
    {
        registry.AddPlugin<HealthPlugin>();
    }
};

std::unique_ptr<Wayfinder::Module> Wayfinder::CreateModule()
{
    return std::make_unique<JourneyModule>();
}
