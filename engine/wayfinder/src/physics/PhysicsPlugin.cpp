#include "PhysicsPlugin.h"
#include "PhysicsComponents.h"
#include "PhysicsSubsystem.h"
#include "app/EngineConfig.h"
#include "core/Log.h"
#include "modules/ModuleRegistry.h"
#include "app/Subsystem.h"
#include "maths/Maths.h"
#include "scene/Components.h"
#include "scene/entity/Entity.h"


#include "ecs/Flecs.h"
#include <nlohmann/json.hpp>

namespace Wayfinder::Physics
{
    // ── Helpers ─────────────────────────────────────────────────

    namespace
    {
        /// Build a PhysicsBodyDescriptor from ECS components.
        PhysicsBodyDescriptor MakeDescriptor(const RigidBodyComponent& rb,
                                             const ColliderComponent& col)
        {
            PhysicsBodyDescriptor desc;
            desc.Type = rb.Type;
            desc.Mass = rb.Mass;
            desc.GravityFactor = rb.GravityFactor;
            desc.LinearDamping = rb.LinearDamping;
            desc.AngularDamping = rb.AngularDamping;
            desc.LinearVelocity = rb.LinearVelocity;
            desc.AngularVelocity = rb.AngularVelocity;

            desc.Shape = col.Shape;
            desc.HalfExtents = col.HalfExtents;
            desc.Radius = col.Radius;
            desc.Height = col.Height;
            desc.Friction = col.Friction;
            desc.Restitution = col.Restitution;
            return desc;
        }
    } // anonymous namespace

    // ── Component registration helpers ──────────────────────────

    namespace
    {
        /// Read a 3-element JSON array into a Float3, falling back to @p fallback.
        Float3 ReadVector3(const nlohmann::json& data, const char* key, const Float3& fallback)
        {
            if (!data.contains(key))
                return fallback;
            const auto& arr = data[key];
            if (!arr.is_array() || arr.size() != 3)
                return fallback;
            return {
                arr[0].is_number() ? arr[0].get<float>() : fallback.x,
                arr[1].is_number() ? arr[1].get<float>() : fallback.y,
                arr[2].is_number() ? arr[2].get<float>() : fallback.z};
        }

        /// Write a Float3 as a 3-element JSON array.
        nlohmann::json WriteVector3(const Float3& value)
        {
            return nlohmann::json::array({value.x, value.y, value.z});
        }

        // --- RigidBodyComponent ---

        void RegisterRigidBody(flecs::world& world)
        {
            world.component<RigidBodyComponent>();
        }

        void ApplyRigidBody(const nlohmann::json& data, Entity& entity)
        {
            RigidBodyComponent rb;

            if (auto it = data.find("type"); it != data.end() && it->is_string())
            {
                auto typeStr = it->get<std::string>();
                if (typeStr == "static")
                    rb.Type = BodyType::Static;
                else if (typeStr == "kinematic")
                    rb.Type = BodyType::Kinematic;
                else
                    rb.Type = BodyType::Dynamic;
            }

            rb.Mass = data.value("mass", 1.0f);
            rb.GravityFactor = data.value("gravity_factor", 1.0f);
            rb.LinearDamping = data.value("linear_damping", 0.05f);
            rb.AngularDamping = data.value("angular_damping", 0.05f);
            rb.LinearVelocity = ReadVector3(data, "linear_velocity", {0.0f, 0.0f, 0.0f});
            rb.AngularVelocity = ReadVector3(data, "angular_velocity", {0.0f, 0.0f, 0.0f});

            entity.AddComponent<RigidBodyComponent>(rb);
        }

        void SerialiseRigidBody(const Entity& entity, nlohmann::json& tables)
        {
            if (!entity.HasComponent<RigidBodyComponent>())
                return;

            const auto& rb = entity.GetComponent<RigidBodyComponent>();
            nlohmann::json t;

            switch (rb.Type)
            {
            case BodyType::Static:
                t["type"] = "static";
                break;
            case BodyType::Dynamic:
                t["type"] = "dynamic";
                break;
            case BodyType::Kinematic:
                t["type"] = "kinematic";
                break;
            }

            t["mass"] = rb.Mass;
            t["gravity_factor"] = rb.GravityFactor;
            t["linear_damping"] = rb.LinearDamping;
            t["angular_damping"] = rb.AngularDamping;
            t["linear_velocity"] = WriteVector3(rb.LinearVelocity);
            t["angular_velocity"] = WriteVector3(rb.AngularVelocity);

            tables["rigid_body"] = std::move(t);
        }

        bool ValidateRigidBody(const nlohmann::json& data, std::string& error)
        {
            if (data.contains("type"))
            {
                const auto& node = data["type"];
                if (!node.is_string())
                {
                    error = "'type' must be a string (static|dynamic|kinematic)";
                    return false;
                }
                auto val = node.get<std::string>();
                if (val != "static" && val != "dynamic" && val != "kinematic")
                {
                    error = "'type' must be one of: static, dynamic, kinematic";
                    return false;
                }
            }
            if (data.contains("mass"))
            {
                const auto& node = data["mass"];
                if (!node.is_number())
                {
                    error = "'mass' must be a number";
                    return false;
                }
                if (node.get<double>() <= 0.0)
                {
                    error = "'mass' must be positive";
                    return false;
                }
            }
            if (data.contains("linear_damping") && !data["linear_damping"].is_number())
            {
                error = "'linear_damping' must be a number";
                return false;
            }
            if (data.contains("angular_damping") && !data["angular_damping"].is_number())
            {
                error = "'angular_damping' must be a number";
                return false;
            }
            return true;
        }

        // --- ColliderComponent ---

        void RegisterCollider(flecs::world& world)
        {
            world.component<ColliderComponent>();
        }

        void ApplyCollider(const nlohmann::json& data, Entity& entity)
        {
            ColliderComponent col;

            if (auto it = data.find("shape"); it != data.end() && it->is_string())
            {
                auto shapeStr = it->get<std::string>();
                if (shapeStr == "sphere")
                    col.Shape = ColliderShape::Sphere;
                else if (shapeStr == "capsule")
                    col.Shape = ColliderShape::Capsule;
                else
                    col.Shape = ColliderShape::Box;
            }

            col.HalfExtents = ReadVector3(data, "half_extents", {0.5f, 0.5f, 0.5f});

            col.Radius = data.value("radius", 0.5f);
            col.Height = data.value("height", 1.0f);
            col.Friction = data.value("friction", 0.2f);
            col.Restitution = data.value("restitution", 0.0f);

            entity.AddComponent<ColliderComponent>(col);
        }

        void SerialiseCollider(const Entity& entity, nlohmann::json& tables)
        {
            if (!entity.HasComponent<ColliderComponent>())
                return;

            const auto& col = entity.GetComponent<ColliderComponent>();
            nlohmann::json t;

            switch (col.Shape)
            {
            case ColliderShape::Box:
                t["shape"] = "box";
                break;
            case ColliderShape::Sphere:
                t["shape"] = "sphere";
                break;
            case ColliderShape::Capsule:
                t["shape"] = "capsule";
                break;
            }

            t["half_extents"] = WriteVector3(col.HalfExtents);
            t["radius"] = col.Radius;
            t["height"] = col.Height;
            t["friction"] = col.Friction;
            t["restitution"] = col.Restitution;

            tables["collider"] = std::move(t);
        }

        bool ValidateCollider(const nlohmann::json& data, std::string& error)
        {
            if (data.contains("shape"))
            {
                const auto& node = data["shape"];
                if (!node.is_string())
                {
                    error = "'shape' must be a string (box|sphere|capsule)";
                    return false;
                }
                auto val = node.get<std::string>();
                if (val != "box" && val != "sphere" && val != "capsule")
                {
                    error = "'shape' must be one of: box, sphere, capsule";
                    return false;
                }
            }
            if (data.contains("friction"))
            {
                const auto& node = data["friction"];
                if (!node.is_number())
                {
                    error = "'friction' must be a number";
                    return false;
                }
                if (node.get<double>() < 0.0)
                {
                    error = "'friction' must be non-negative";
                    return false;
                }
            }
            if (data.contains("restitution"))
            {
                const auto& node = data["restitution"];
                if (!node.is_number())
                {
                    error = "'restitution' must be a number";
                    return false;
                }
                if (node.get<double>() < 0.0)
                {
                    error = "'restitution' must be non-negative";
                    return false;
                }
            }
            if (data.contains("radius"))
            {
                const auto& node = data["radius"];
                if (!node.is_number())
                {
                    error = "'radius' must be a number";
                    return false;
                }
                if (node.get<double>() <= 0.0)
                {
                    error = "'radius' must be positive";
                    return false;
                }
            }
            if (data.contains("height"))
            {
                const auto& node = data["height"];
                if (!node.is_number())
                {
                    error = "'height' must be a number";
                    return false;
                }
                if (node.get<double>() <= 0.0)
                {
                    error = "'height' must be positive";
                    return false;
                }
            }
            return true;
        }
    } // anonymous namespace

    // ── Plugin build ────────────────────────────────────────────

    void PhysicsPlugin::Build(ModuleRegistry& registry)
    {
        // --- Subsystem ---
        registry.RegisterSubsystem<PhysicsSubsystem>();

        // Read the configured fixed timestep so the system factory can apply it.
        const float fixedTimestep = registry.GetConfig().Physics.FixedTimestep;

        // --- Components ---
        {
            ModuleRegistry::ComponentDescriptor desc;
            desc.Key = "rigid_body";
            desc.RegisterFn = &RegisterRigidBody;
            desc.ApplyFn = &ApplyRigidBody;
            desc.SerialiseFn = &SerialiseRigidBody;
            desc.ValidateFn = &ValidateRigidBody;
            registry.RegisterComponent(std::move(desc));
        }
        {
            ModuleRegistry::ComponentDescriptor desc;
            desc.Key = "collider";
            desc.RegisterFn = &RegisterCollider;
            desc.ApplyFn = &ApplyCollider;
            desc.SerialiseFn = &SerialiseCollider;
            desc.ValidateFn = &ValidateCollider;
            registry.RegisterComponent(std::move(desc));
        }

        // --- ECS Observers & Systems ---

        // PhysicsCreateBodies: system that creates a Jolt body for every entity
        // that has a complete physics description (RigidBody + Collider + Transform)
        // but no runtime body yet.  Runs in PreUpdate so bodies exist before
        // PhysicsStep simulates them.
        registry.RegisterSystem("PhysicsCreateBodies", [](flecs::world& world) {
            world.system<RigidBodyComponent, const ColliderComponent, const TransformComponent>("PhysicsCreateBodies")
                .kind(flecs::PreUpdate)
                .each([](flecs::entity,
                         RigidBodyComponent& rb,
                         const ColliderComponent& col,
                         const TransformComponent& transform) {
                    if (rb.RuntimeBodyId != INVALID_PHYSICS_BODY)
                        return;

                    auto* physics = GameSubsystems::Find<PhysicsSubsystem>();
                    if (!physics)
                        return;

                    auto desc = MakeDescriptor(rb, col);
                    rb.RuntimeBodyId = physics->GetWorld().CreateBody(
                        desc, transform.Position, transform.Rotation);
                });
        });

        // PhysicsDestroyBodies: reactive observer that destroys the Jolt body
        // when RigidBodyComponent is removed from an entity (or entity deleted).
        // Fires before the component data is destroyed, so RuntimeBodyId is
        // still accessible.
        //
        // NOTE: This observer deliberately does a dynamic GameSubsystems lookup
        // rather than caching the pointer, because OnRemove fires during
        // flecs::world destruction — which may happen after the subsystem
        // collection has been shut down and the PhysicsSubsystem destroyed.
        registry.RegisterSystem("PhysicsDestroyBodies", [](flecs::world& world) {
            world.observer<RigidBodyComponent>("PhysicsDestroyBodies")
                .event(flecs::OnRemove)
                .each([](flecs::entity, RigidBodyComponent& rb) {
                    if (rb.RuntimeBodyId == INVALID_PHYSICS_BODY)
                        return;

                    auto* physics = GameSubsystems::Find<PhysicsSubsystem>();
                    if (!physics)
                        return;

                    physics->GetWorld().DestroyBody(rb.RuntimeBodyId);
                    rb.RuntimeBodyId = INVALID_PHYSICS_BODY;
                });
        });

        // PhysicsStep: advance the Jolt simulation using a fixed timestep
        // accumulator.  The configured timestep is captured here and applied
        // to the PhysicsWorld once during system registration (which runs
        // after subsystem initialisation).
        registry.RegisterSystem("PhysicsStep", [fixedTimestep](flecs::world& world) {
            auto* physics = GameSubsystems::Find<PhysicsSubsystem>();
            if (physics)
                physics->GetWorld().SetFixedTimestep(fixedTimestep);

            world.system("PhysicsStep")
                .kind(flecs::OnUpdate)
                .run([physics](flecs::iter& it) {
                    if (physics)
                        physics->GetWorld().StepFixed(it.delta_time());
                });
        });

        // PhysicsSyncTransforms: copy Jolt body position and rotation into
        // WorldTransformComponent.  Builds the full LocalToWorld matrix from
        // physics position, physics rotation, and the entity's existing scale.
        registry.RegisterSystem("PhysicsSyncTransforms", [](flecs::world& world) 
        {
            auto* physics = GameSubsystems::Find<PhysicsSubsystem>();

            world.system<const RigidBodyComponent, WorldTransformComponent>("PhysicsSyncTransforms")
            .kind(flecs::OnValidate)
            .each([physics](flecs::entity, const RigidBodyComponent& rb, WorldTransformComponent& wt) 
            {
                if (rb.RuntimeBodyId == INVALID_PHYSICS_BODY)
                    return;
                if (rb.Type == BodyType::Static)
                    return;
                if (!physics)
                    return;

                Float3 pos = physics->GetWorld().GetBodyPosition(rb.RuntimeBodyId);
                Float4 rotQ = physics->GetWorld().GetBodyRotation(rb.RuntimeBodyId);

                wt.Position = pos;

                // Build LocalToWorld = translate * rotate * scale.
                Quaternion q(rotQ.w, rotQ.x, rotQ.y, rotQ.z);
                Matrix4 rotMat = Maths::ToMatrix4(q);
                Matrix4 translateMat = Maths::Translate(Matrix4(1.0f), pos);
                Matrix4 scaleMat = Maths::ScaleMatrix(Matrix4(1.0f), wt.Scale);
                wt.LocalToWorld = translateMat * rotMat * scaleMat;
            });
        },
        {}, {"PhysicsStep"});

        WAYFINDER_INFO(LogPhysics, "PhysicsPlugin registered");
    }

} // namespace Wayfinder::Physics
