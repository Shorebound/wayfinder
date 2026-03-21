#include "PhysicsPlugin.h"
#include "PhysicsComponents.h"
#include "PhysicsSubsystem.h"
#include "../core/Log.h"
#include "../core/ModuleRegistry.h"
#include "../core/Subsystem.h"
#include "../scene/Components.h"
#include "../scene/entity/Entity.h"

#include <flecs.h>
#include <toml++/toml.hpp>

namespace Wayfinder
{
    // ── Component registration helpers ──────────────────────────

    namespace
    {
        // --- RigidBodyComponent ---

        void RegisterRigidBody(flecs::world& world)
        {
            world.component<RigidBodyComponent>();
        }

        void ApplyRigidBody(const toml::table& table, Entity& entity)
        {
            RigidBodyComponent rb;

            if (auto typeStr = table["type"].value<std::string>())
            {
                if (*typeStr == "static")
                    rb.Type = BodyType::Static;
                else if (*typeStr == "kinematic")
                    rb.Type = BodyType::Kinematic;
                else
                    rb.Type = BodyType::Dynamic;
            }

            rb.Mass = table["mass"].value_or(1.0f);
            rb.GravityFactor = table["gravity_factor"].value_or(1.0f);

            if (auto vel = table["linear_velocity"].as_array(); vel && vel->size() == 3)
            {
                rb.LinearVelocity = {
                    vel->get(0)->value_or(0.0f),
                    vel->get(1)->value_or(0.0f),
                    vel->get(2)->value_or(0.0f)};
            }

            if (auto vel = table["angular_velocity"].as_array(); vel && vel->size() == 3)
            {
                rb.AngularVelocity = {
                    vel->get(0)->value_or(0.0f),
                    vel->get(1)->value_or(0.0f),
                    vel->get(2)->value_or(0.0f)};
            }

            entity.AddComponent<RigidBodyComponent>(rb);
        }

        void SerialiseRigidBody(const Entity& entity, toml::table& tables)
        {
            if (!entity.HasComponent<RigidBodyComponent>())
                return;

            const auto& rb = entity.GetComponent<RigidBodyComponent>();
            toml::table t;

            switch (rb.Type)
            {
            case BodyType::Static:
                t.insert("type", "static");
                break;
            case BodyType::Dynamic:
                t.insert("type", "dynamic");
                break;
            case BodyType::Kinematic:
                t.insert("type", "kinematic");
                break;
            }

            t.insert("mass", rb.Mass);
            t.insert("gravity_factor", rb.GravityFactor);
            t.insert("linear_velocity", toml::array{rb.LinearVelocity.x, rb.LinearVelocity.y, rb.LinearVelocity.z});
            t.insert("angular_velocity", toml::array{rb.AngularVelocity.x, rb.AngularVelocity.y, rb.AngularVelocity.z});

            tables.insert("rigid_body", std::move(t));
        }

        bool ValidateRigidBody(const toml::table& table, std::string& error)
        {
            if (const auto* node = table.get("type"); node && !node->is_string())
            {
                error = "'type' must be a string (static|dynamic|kinematic)";
                return false;
            }
            if (const auto* node = table.get("mass"); node && !node->is_floating_point() && !node->is_integer())
            {
                error = "'mass' must be a number";
                return false;
            }
            return true;
        }

        // --- ColliderComponent ---

        void RegisterCollider(flecs::world& world)
        {
            world.component<ColliderComponent>();
        }

        void ApplyCollider(const toml::table& table, Entity& entity)
        {
            ColliderComponent col;

            if (auto shapeStr = table["shape"].value<std::string>())
            {
                if (*shapeStr == "sphere")
                    col.Shape = ColliderShape::Sphere;
                else
                    col.Shape = ColliderShape::Box;
            }

            if (auto ext = table["half_extents"].as_array(); ext && ext->size() == 3)
            {
                col.HalfExtents = {
                    ext->get(0)->value_or(0.5f),
                    ext->get(1)->value_or(0.5f),
                    ext->get(2)->value_or(0.5f)};
            }

            col.Radius = table["radius"].value_or(0.5f);
            col.Friction = table["friction"].value_or(0.2f);
            col.Restitution = table["restitution"].value_or(0.0f);

            entity.AddComponent<ColliderComponent>(col);
        }

        void SerialiseCollider(const Entity& entity, toml::table& tables)
        {
            if (!entity.HasComponent<ColliderComponent>())
                return;

            const auto& col = entity.GetComponent<ColliderComponent>();
            toml::table t;

            t.insert("shape", col.Shape == ColliderShape::Sphere ? "sphere" : "box");
            t.insert("half_extents", toml::array{col.HalfExtents.x, col.HalfExtents.y, col.HalfExtents.z});
            t.insert("radius", col.Radius);
            t.insert("friction", col.Friction);
            t.insert("restitution", col.Restitution);

            tables.insert("collider", std::move(t));
        }

        bool ValidateCollider(const toml::table& table, std::string& error)
        {
            if (const auto* node = table.get("shape"); node && !node->is_string())
            {
                error = "'shape' must be a string (box|sphere)";
                return false;
            }
            if (const auto* node = table.get("friction"); node && !node->is_floating_point() && !node->is_integer())
            {
                error = "'friction' must be a number";
                return false;
            }
            return true;
        }
    } // anonymous namespace

    // ── Plugin build ────────────────────────────────────────────

    void PhysicsPlugin::Build(ModuleRegistry& registry)
    {
        // --- Subsystem ---
        registry.RegisterSubsystem<PhysicsSubsystem>();

        // --- Components ---
        {
            ModuleRegistry::ComponentDescriptor desc;
            desc.Key = "rigid_body";
            desc.RegisterFn = &RegisterRigidBody;
            desc.ApplyFn = &ApplyRigidBody;
            desc.SerializeFn = &SerialiseRigidBody;
            desc.ValidateFn = &ValidateRigidBody;
            registry.RegisterComponent(std::move(desc));
        }
        {
            ModuleRegistry::ComponentDescriptor desc;
            desc.Key = "collider";
            desc.RegisterFn = &RegisterCollider;
            desc.ApplyFn = &ApplyCollider;
            desc.SerializeFn = &SerialiseCollider;
            desc.ValidateFn = &ValidateCollider;
            registry.RegisterComponent(std::move(desc));
        }

        // --- ECS Systems ---

        // PhysicsSync: create Jolt bodies for entities that have the
        // required components but no valid RuntimeBodyId yet.
        registry.RegisterSystem("PhysicsSync", [](flecs::world& world)
        {
            world.system<RigidBodyComponent, const ColliderComponent, const TransformComponent>("PhysicsSync")
                .kind(flecs::PreUpdate)
                .each([](flecs::entity e,
                         RigidBodyComponent& rb,
                         const ColliderComponent& col,
                         const TransformComponent& transform)
                {
                    if (rb.RuntimeBodyId != INVALID_PHYSICS_BODY)
                        return;

                    auto* subsystem = GameSubsystems::Find<PhysicsSubsystem>();
                    if (!subsystem)
                        return;

                    rb.RuntimeBodyId = subsystem->GetWorld().CreateBody(rb, col, transform.Position);
                });
        });

        // PhysicsStep: advance the Jolt simulation once per frame.
        registry.RegisterSystem("PhysicsStep", [](flecs::world& world)
        {
            world.system("PhysicsStep")
                .kind(flecs::OnUpdate)
                .iter([](flecs::iter& it)
                {
                    auto* subsystem = GameSubsystems::Find<PhysicsSubsystem>();
                    if (!subsystem)
                        return;

                    subsystem->GetWorld().Step(it.delta_time());
                });
        }, {}, {"PhysicsSync"});

        // PhysicsWriteback: copy Jolt body positions into WorldTransformComponent.
        registry.RegisterSystem("PhysicsWriteback", [](flecs::world& world)
        {
            world.system<const RigidBodyComponent, WorldTransformComponent>("PhysicsWriteback")
                .kind(flecs::OnValidate)
                .each([](flecs::entity e,
                         const RigidBodyComponent& rb,
                         WorldTransformComponent& wt)
                {
                    if (rb.RuntimeBodyId == INVALID_PHYSICS_BODY)
                        return;
                    if (rb.Type == BodyType::Static)
                        return;

                    auto* subsystem = GameSubsystems::Find<PhysicsSubsystem>();
                    if (!subsystem)
                        return;

                    Float3 pos = subsystem->GetWorld().GetBodyPosition(rb.RuntimeBodyId);
                    wt.Position = pos;
                    wt.LocalToWorld[3] = Float4(pos, 1.0f);
                });
        }, {}, {"PhysicsStep"});

        WAYFINDER_INFO(LogPhysics, "PhysicsPlugin registered");
    }

} // namespace Wayfinder
