#include "PhysicsPlugin.h"
#include "PhysicsComponents.h"
#include "PhysicsSubsystem.h"
#include "app/EngineConfig.h"
#include "app/Subsystem.h"
#include "core/Log.h"
#include "maths/Maths.h"
#include "plugins/PluginRegistry.h"
#include "scene/Components.h"
#include "scene/entity/Entity.h"

#include "ecs/Flecs.h"

#include <array>
#include <bit>
#include <nlohmann/json.hpp>

namespace Wayfinder::Physics
{
    // ── Helpers ─────────────────────────────────────────────────

    namespace
    {
        [[nodiscard]] nlohmann::json::const_iterator FindJsonMember(const nlohmann::json& data, const char* key)
        {
            return data.find(key);
        }

        [[nodiscard]] std::array<float, 3> GetFloat3Components(const Float3& value)
        {
            static_assert(sizeof(Float3) == sizeof(std::array<float, 3>));
            return std::bit_cast<std::array<float, 3>>(value);
        }

        [[nodiscard]] std::array<float, 4> GetFloat4Components(const Float4& value)
        {
            static_assert(sizeof(Float4) == sizeof(std::array<float, 4>));
            return std::bit_cast<std::array<float, 4>>(value);
        }

        void AssignJsonMember(nlohmann::json& object, const char* key, nlohmann::json value)
        {
            object.erase(key);
            object.emplace(key, std::move(value));
        }

        [[nodiscard]] Quaternion ToQuaternion(const Float4& rotation)
        {
            const auto components = GetFloat4Components(rotation);
            return {components.at(3), components.at(0), components.at(1), components.at(2)};
        }

        /// Build a PhysicsBodyDescriptor from ECS components.
        PhysicsBodyDescriptor MakeDescriptor(const RigidBodyComponent& rb, const ColliderComponent& col)
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
            const auto it = FindJsonMember(data, key);
            if (it == data.end())
            {
                return fallback;
            }
            const auto& arr = *it;
            if (!arr.is_array() || arr.size() != 3)
            {
                return fallback;
            }

            const auto& xNode = arr.at(0);
            const auto& yNode = arr.at(1);
            const auto& zNode = arr.at(2);
            const auto fallbackComponents = GetFloat3Components(fallback);

            return {
                xNode.is_number() ? xNode.get<float>() : fallbackComponents.at(0), yNode.is_number() ? yNode.get<float>() : fallbackComponents.at(1), zNode.is_number() ? zNode.get<float>() : fallbackComponents.at(2)};
        }

        /// Write a Float3 as a 3-element JSON array.
        nlohmann::json WriteVector3(const Float3& value)
        {
            const auto components = GetFloat3Components(value);
            return nlohmann::json::array({components.at(0), components.at(1), components.at(2)});
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
                {
                    rb.Type = BodyType::Static;
                }
                else if (typeStr == "kinematic")
                {
                    rb.Type = BodyType::Kinematic;
                }
                else
                {
                    rb.Type = BodyType::Dynamic;
                }
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
            {
                return;
            }

            const auto& rb = entity.GetComponent<RigidBodyComponent>();
            nlohmann::json t;

            switch (rb.Type)
            {
            case BodyType::Static:
                AssignJsonMember(t, "type", "static");
                break;
            case BodyType::Dynamic:
                AssignJsonMember(t, "type", "dynamic");
                break;
            case BodyType::Kinematic:
                AssignJsonMember(t, "type", "kinematic");
                break;
            }

            AssignJsonMember(t, "mass", rb.Mass);
            AssignJsonMember(t, "gravity_factor", rb.GravityFactor);
            AssignJsonMember(t, "linear_damping", rb.LinearDamping);
            AssignJsonMember(t, "angular_damping", rb.AngularDamping);
            AssignJsonMember(t, "linear_velocity", WriteVector3(rb.LinearVelocity));
            AssignJsonMember(t, "angular_velocity", WriteVector3(rb.AngularVelocity));

            AssignJsonMember(tables, "rigid_body", std::move(t));
        }

        bool ValidateRigidBody(const nlohmann::json& data, std::string& error)
        {
            if (const auto it = FindJsonMember(data, "type"); it != data.end())
            {
                const auto& node = *it;
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
            if (const auto it = FindJsonMember(data, "mass"); it != data.end())
            {
                const auto& node = *it;
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
            if (const auto it = FindJsonMember(data, "linear_damping"); it != data.end() && !it->is_number())
            {
                error = "'linear_damping' must be a number";
                return false;
            }
            if (const auto it = FindJsonMember(data, "angular_damping"); it != data.end() && !it->is_number())
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
                {
                    col.Shape = ColliderShape::Sphere;
                }
                else if (shapeStr == "capsule")
                {
                    col.Shape = ColliderShape::Capsule;
                }
                else
                {
                    col.Shape = ColliderShape::Box;
                }
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
            {
                return;
            }

            const auto& col = entity.GetComponent<ColliderComponent>();
            nlohmann::json t;

            switch (col.Shape)
            {
            case ColliderShape::Box:
                AssignJsonMember(t, "shape", "box");
                break;
            case ColliderShape::Sphere:
                AssignJsonMember(t, "shape", "sphere");
                break;
            case ColliderShape::Capsule:
                AssignJsonMember(t, "shape", "capsule");
                break;
            }

            AssignJsonMember(t, "half_extents", WriteVector3(col.HalfExtents));
            AssignJsonMember(t, "radius", col.Radius);
            AssignJsonMember(t, "height", col.Height);
            AssignJsonMember(t, "friction", col.Friction);
            AssignJsonMember(t, "restitution", col.Restitution);

            AssignJsonMember(tables, "collider", std::move(t));
        }

        bool ValidateCollider(const nlohmann::json& data, std::string& error)
        {
            if (const auto it = FindJsonMember(data, "shape"); it != data.end())
            {
                const auto& node = *it;
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
            if (const auto it = FindJsonMember(data, "friction"); it != data.end())
            {
                const auto& node = *it;
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
            if (const auto it = FindJsonMember(data, "restitution"); it != data.end())
            {
                const auto& node = *it;
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
            if (const auto it = FindJsonMember(data, "radius"); it != data.end())
            {
                const auto& node = *it;
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
            if (const auto it = FindJsonMember(data, "height"); it != data.end())
            {
                const auto& node = *it;
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

    void PhysicsPlugin::Build(Plugins::PluginRegistry& registry)
    {
        // --- Subsystem ---
        registry.RegisterSubsystem<PhysicsSubsystem>();

        // Read the configured fixed timestep so the system factory can apply it.
        const float fixedTimestep = registry.GetConfig().Physics.FixedTimestep;

        // --- Components ---
        {
            Plugins::PluginRegistry::ComponentDescriptor desc;
            desc.Key = "rigid_body";
            desc.RegisterFn = &RegisterRigidBody;
            desc.ApplyFn = &ApplyRigidBody;
            desc.SerialiseFn = &SerialiseRigidBody;
            desc.ValidateFn = &ValidateRigidBody;
            registry.RegisterComponent(std::move(desc));
        }
        {
            Plugins::PluginRegistry::ComponentDescriptor desc;
            desc.Key = "collider";
            desc.RegisterFn = &RegisterCollider;
            desc.ApplyFn = &ApplyCollider;
            desc.SerialiseFn = &SerialiseCollider;
            desc.ValidateFn = &ValidateCollider;
            registry.RegisterComponent(std::move(desc));
        }

        // --- ECS Observers & Systems ---

        // PhysicsCreateBodies: reactive observer that creates a Jolt body when
        // an entity gains the full physics archetype (RigidBody + Collider +
        // Transform).  OnAdd fires once all three matched components are
        // present.  The RuntimeBodyId guard prevents double-creation if the
        // observer re-fires for the same entity.
        registry.RegisterSystem("PhysicsCreateBodies", [](flecs::world& world)
        {
            world.observer<RigidBodyComponent, const ColliderComponent, const TransformComponent>("PhysicsCreateBodies")
                .event(flecs::OnAdd)
                .each([](flecs::entity, RigidBodyComponent& rb, const ColliderComponent& col, const TransformComponent& transform)
            {
                if (rb.RuntimeBodyId != INVALID_PHYSICS_BODY)
                {
                    return;
                }

                auto* physics = GameSubsystems::Find<PhysicsSubsystem>();
                if (!physics)
                {
                    return;
                }

                auto desc = MakeDescriptor(rb, col);
                const PhysicsBodyPose pose{
                    .Position = transform.Local.Position,
                    .RotationDegrees = transform.Local.RotationDegrees,
                };

                rb.RuntimeBodyId = physics->GetWorld().CreateBody(desc, pose);
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
        registry.RegisterSystem("PhysicsDestroyBodies", [](flecs::world& world)
        {
            world.observer<RigidBodyComponent>("PhysicsDestroyBodies")
                .event(flecs::OnRemove)
                .each([](RigidBodyComponent& rb)
            {
                if (rb.RuntimeBodyId == INVALID_PHYSICS_BODY)
                {
                    return;
                }

                auto* physics = GameSubsystems::Find<PhysicsSubsystem>();
                if (!physics)
                {
                    return;
                }

                physics->GetWorld().DestroyBody(rb.RuntimeBodyId);
                rb.RuntimeBodyId = INVALID_PHYSICS_BODY;
            });
        });

        // PhysicsStep: advance the Jolt simulation using a fixed timestep
        // accumulator.  The configured timestep is captured here and applied
        // to the PhysicsWorld once during system registration (which runs
        // after subsystem initialisation).
        registry.RegisterSystem("PhysicsStep", [fixedTimestep](flecs::world& world)
        {
            auto* physics = GameSubsystems::Find<PhysicsSubsystem>();
            if (physics)
            {
                physics->GetWorld().SetFixedTimestep(fixedTimestep);
            }

            world.system("PhysicsStep")
                .kind(flecs::OnUpdate)
                .run([physics](flecs::iter& it)
            {
                if (physics)
                {
                    physics->GetWorld().StepFixed(it.delta_time());
                }
            });
        });

        // PhysicsSyncTransforms: copy Jolt body position and rotation into
        // WorldTransformComponent.  Builds the full LocalToWorld matrix from
        // physics position, physics rotation, and the entity's existing scale.
        registry.RegisterSystem("PhysicsSyncTransforms", [](flecs::world& world)
        {
            auto* physics = GameSubsystems::Find<PhysicsSubsystem>();

            // Flecs' system builder triggers a known false positive in clang's
            // stack-escape analyzer through third-party template internals.
            // NOLINTNEXTLINE(clang-analyzer-core.StackAddressEscape)
            world.system<>("PhysicsSyncTransforms")
                .kind(flecs::OnValidate)
                .run([physics, &world](flecs::iter&)
            {
                if (!physics)
                {
                    return;
                }

                world.each([physics](flecs::entity, const RigidBodyComponent& rb, WorldTransformComponent& wt)
                {
                    if (rb.RuntimeBodyId == INVALID_PHYSICS_BODY)
                    {
                        return;
                    }
                    if (rb.Type == BodyType::Static)
                    {
                        return;
                    }

                    const Float3 pos = physics->GetWorld().GetBodyPosition(rb.RuntimeBodyId);
                    const Float4 rotQ = physics->GetWorld().GetBodyRotation(rb.RuntimeBodyId);

                    wt.Position = pos;

                    // Build LocalToWorld = translate * rotate * scale.
                    const Quaternion q = ToQuaternion(rotQ);
                    const Matrix4 rotMat = Maths::ToMatrix4(q);
                    const Matrix4 translateMat = Maths::Translate(Matrix4(1.0f), pos);
                    const Matrix4 scaleMat = Maths::ScaleMatrix(Matrix4(1.0f), wt.Scale);
                    wt.LocalToWorld = translateMat * rotMat * scaleMat;
                });
            });
        }, {}, {"PhysicsStep"});

        WAYFINDER_INFO(LogPhysics, "PhysicsPlugin registered");
    }

} // namespace Wayfinder::Physics
