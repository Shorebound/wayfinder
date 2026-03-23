#include "PhysicsWorld.h"
#include "core/Log.h"
#include "maths/Maths.h"

// Jolt includes — Jolt.h must come first.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <array>
#include <bit>
#include <mutex>

namespace Wayfinder::Physics
{
    // ── Jolt layer configuration ────────────────────────────────

    namespace PhysicsLayers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr uint32_t NUM_LAYERS = 2;
    } // namespace PhysicsLayers

    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr uint32_t NUM_LAYERS = 2;
    } // namespace BroadPhaseLayers

    namespace
    {
        [[nodiscard]] std::array<float, 3> GetFloat3Components(const Float3& value)
        {
            static_assert(sizeof(Float3) == sizeof(std::array<float, 3>));
            return std::bit_cast<std::array<float, 3>>(value);
        }

        [[nodiscard]] constexpr std::size_t ToLayerIndex(JPH::ObjectLayer layer)
        {
            return static_cast<std::size_t>(layer);
        }

        [[nodiscard]] JPH::Vec3 ToJoltVec3(const Float3& value)
        {
            const auto components = GetFloat3Components(value);
            return {components.at(0), components.at(1), components.at(2)};
        }

        [[nodiscard]] JPH::RVec3 ToJoltRVec3(const Float3& value)
        {
            const auto components = GetFloat3Components(value);
            return {components.at(0), components.at(1), components.at(2)};
        }

        [[nodiscard]] JPH::Quat ToJoltEulerRotation(const Float3& rotationDegrees)
        {
            const auto components = GetFloat3Components(rotationDegrees);
            const float rx = Maths::ToRadians(components.at(0));
            const float ry = Maths::ToRadians(components.at(1));
            const float rz = Maths::ToRadians(components.at(2));
            return JPH::Quat::sEulerAngles(JPH::Vec3(rx, ry, rz));
        }

        class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
        {
        public:
            BPLayerInterfaceImpl()
            {
                m_objectToBroadPhase.at(ToLayerIndex(PhysicsLayers::NON_MOVING)) = BroadPhaseLayers::NON_MOVING;
                m_objectToBroadPhase.at(ToLayerIndex(PhysicsLayers::MOVING)) = BroadPhaseLayers::MOVING;
            }

            [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override
            {
                return BroadPhaseLayers::NUM_LAYERS;
            }

            [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
            {
                JPH_ASSERT(inLayer < PhysicsLayers::NUM_LAYERS);
                if (inLayer >= PhysicsLayers::NUM_LAYERS)
                {
                    return BroadPhaseLayers::NON_MOVING;
                }
                return m_objectToBroadPhase.at(ToLayerIndex(inLayer));
            }

            [[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
            {
                switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer))
                {
                case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::NON_MOVING):
                    return "NON_MOVING";
                case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::MOVING):
                    return "MOVING";
                default:
                    return "UNKNOWN";
                }
            }

        private:
            std::array<JPH::BroadPhaseLayer, PhysicsLayers::NUM_LAYERS> m_objectToBroadPhase{};
        };

        class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
        public:
            [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
            {
                switch (inLayer1)
                {
                case PhysicsLayers::NON_MOVING:
                    return inLayer2 == BroadPhaseLayers::MOVING;
                case PhysicsLayers::MOVING:
                    return true;
                default:
                    return false;
                }
            }
        };

        class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
        {
        public:
            [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override
            {
                switch (inLayer1)
                {
                case PhysicsLayers::NON_MOVING:
                    return inLayer2 == PhysicsLayers::MOVING;
                case PhysicsLayers::MOVING:
                    return true;
                default:
                    return false;
                }
            }
        };

        std::once_flag g_joltInitFlag;

        void InitialiseJoltGlobals()
        {
            std::call_once(g_joltInitFlag, []()
            {
                JPH::RegisterDefaultAllocator();
                JPH::Factory::sInstance = new JPH::Factory();
                JPH::RegisterTypes();

                // Register a one-time teardown so Jolt globals are cleaned up
                // when the process exits (avoids leak reports in sanitisers).
                std::atexit([]()
                {
                    JPH::UnregisterTypes();
                    delete JPH::Factory::sInstance;
                    JPH::Factory::sInstance = nullptr;
                });
            });
        }
    } // anonymous namespace

    // ── PhysicsWorld implementation detail ───────────────────────

    struct PhysicsWorld::Impl
    {
        static constexpr uint32_t MAX_BODIES = 1024;
        static constexpr uint32_t NUM_BODY_MUTEXES = 0;
        static constexpr uint32_t MAX_BODY_PAIRS = 1024;
        static constexpr uint32_t MAX_CONTACT_CONSTRAINTS = 1024;
        static constexpr uint32_t TEMP_ALLOCATOR_SIZE = 10 * 1024 * 1024; // 10 MB

        std::unique_ptr<JPH::PhysicsSystem> PhysSystem;
        std::unique_ptr<JPH::TempAllocatorImpl> TempAlloc;
        std::unique_ptr<JPH::JobSystemSingleThreaded> JobSys;

        BPLayerInterfaceImpl BroadPhaseLayerIface;
        ObjectVsBroadPhaseLayerFilterImpl ObjVsBpFilter;
        ObjectLayerPairFilterImpl ObjPairFilter;
    };

    // ── PhysicsWorld public API ─────────────────────────────────

    PhysicsWorld::PhysicsWorld() = default;
    PhysicsWorld::~PhysicsWorld()
    {
        if (m_initialised)
        {
            Shutdown();
        }
    }

    void PhysicsWorld::Initialise()
    {
        if (m_initialised)
        {
            return;
        }

        InitialiseJoltGlobals();

        m_impl = std::make_unique<Impl>();
        m_impl->TempAlloc = std::make_unique<JPH::TempAllocatorImpl>(Impl::TEMP_ALLOCATOR_SIZE);
        m_impl->JobSys = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);

        m_impl->PhysSystem = std::make_unique<JPH::PhysicsSystem>();
        m_impl->PhysSystem->Init(Impl::MAX_BODIES, Impl::NUM_BODY_MUTEXES, Impl::MAX_BODY_PAIRS, Impl::MAX_CONTACT_CONSTRAINTS, m_impl->BroadPhaseLayerIface, m_impl->ObjVsBpFilter, m_impl->ObjPairFilter);

        m_accumulator = 0.0f;
        m_initialised = true;
        WAYFINDER_INFO(LogPhysics, "PhysicsWorld initialised (Jolt, fixed dt={:.4f}s)", m_fixedTimestep);
    }

    void PhysicsWorld::Shutdown()
    {
        if (!m_initialised)
        {
            return;
        }

        m_impl.reset();
        m_initialised = false;
        WAYFINDER_INFO(LogPhysics, "PhysicsWorld shut down");
    }

    void PhysicsWorld::Step(float deltaTime)
    {
        if (!m_initialised || deltaTime <= 0.0f)
        {
            return;
        }

        constexpr int COLLISION_STEPS = 1;
        m_impl->PhysSystem->Update(deltaTime, COLLISION_STEPS, m_impl->TempAlloc.get(), m_impl->JobSys.get());
    }

    int PhysicsWorld::StepFixed(float frameDeltaTime)
    {
        if (!m_initialised || frameDeltaTime <= 0.0f)
        {
            return 0;
        }

        m_accumulator += frameDeltaTime;

        // Cap the accumulator to avoid a spiral-of-death when a frame takes
        // much longer than expected (e.g. debugger pause, long hitch).
        constexpr float MAX_ACCUMULATED = 0.25f;
        if (m_accumulator > MAX_ACCUMULATED)
        {
            m_accumulator = MAX_ACCUMULATED;
        }

        int steps = 0;
        while (m_accumulator >= m_fixedTimestep)
        {
            Step(m_fixedTimestep);
            m_accumulator -= m_fixedTimestep;
            ++steps;
        }

        return steps;
    }

    void PhysicsWorld::SetFixedTimestep(float timestep)
    {
        if (timestep <= 0.0f)
        {
            WAYFINDER_WARNING(LogPhysics, "SetFixedTimestep called with non-positive value {:.6f}; ignoring", timestep);
            return;
        }
        m_fixedTimestep = timestep;
    }

    uint32_t PhysicsWorld::CreateBody(const PhysicsBodyDescriptor& desc, const PhysicsBodyPose& pose)
    {
        if (!m_initialised)
        {
            return INVALID_PHYSICS_BODY;
        }

        // Build shape
        JPH::Ref<JPH::Shape> shape;
        switch (desc.Shape)
        {
        case ColliderShape::Box:
            shape = new JPH::BoxShape(ToJoltVec3(desc.HalfExtents));
            break;
        case ColliderShape::Sphere:
            shape = new JPH::SphereShape(desc.Radius);
            break;
        case ColliderShape::Capsule:
            shape = new JPH::CapsuleShape(desc.Height * 0.5f, desc.Radius);
            break;
        default:
            WAYFINDER_ERROR(LogPhysics, "Unknown ColliderShape");
            return INVALID_PHYSICS_BODY;
        }

        // Map BodyType → Jolt motion type and object layer
        JPH::EMotionType motionType = JPH::EMotionType::Dynamic;
        JPH::ObjectLayer objectLayer = PhysicsLayers::MOVING;
        switch (desc.Type)
        {
        case BodyType::Static:
            motionType = JPH::EMotionType::Static;
            objectLayer = PhysicsLayers::NON_MOVING;
            break;
        case BodyType::Dynamic:
            motionType = JPH::EMotionType::Dynamic;
            objectLayer = PhysicsLayers::MOVING;
            break;
        case BodyType::Kinematic:
            motionType = JPH::EMotionType::Kinematic;
            objectLayer = PhysicsLayers::MOVING;
            break;
        }

        // Convert Euler ZYX degrees to Jolt quaternion.
        // Jolt's sEulerAngles applies rotations in X-Y-Z intrinsic order.
        // TransformComponent.Local.RotationDegrees stores degrees in the same convention
        // as Maths::ComposeTransform (Z-Y-X extrinsic = X-Y-Z intrinsic).
        const JPH::Quat rotation = ToJoltEulerRotation(pose.RotationDegrees);

        JPH::BodyCreationSettings settings(shape, ToJoltRVec3(pose.Position), rotation, motionType, objectLayer);

        // Material properties apply to both dynamic and kinematic bodies.
        settings.mFriction = desc.Friction;
        settings.mRestitution = desc.Restitution;

        if (desc.Type == BodyType::Dynamic)
        {
            settings.mGravityFactor = desc.GravityFactor;
            settings.mLinearDamping = desc.LinearDamping;
            settings.mAngularDamping = desc.AngularDamping;
            settings.mLinearVelocity = ToJoltVec3(desc.LinearVelocity);
            settings.mAngularVelocity = ToJoltVec3(desc.AngularVelocity);

            // Override mass if a positive value was provided.
            // Mass <= 0 means "use shape-computed mass".
            if (desc.Mass > 0.0f)
            {
                settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                settings.mMassPropertiesOverride.mMass = desc.Mass;
            }
        }

        JPH::BodyInterface& bodyInterface = m_impl->PhysSystem->GetBodyInterface();
        const JPH::Body* joltBody = bodyInterface.CreateBody(settings);
        if (!joltBody)
        {
            const auto positionComponents = GetFloat3Components(pose.Position);
            WAYFINDER_ERROR(LogPhysics, "Failed to create Jolt body (type={}, shape={}, pos=[{},{},{}])", static_cast<int>(desc.Type), static_cast<int>(desc.Shape), positionComponents.at(0), positionComponents.at(1),
                positionComponents.at(2));
            return INVALID_PHYSICS_BODY;
        }

        const JPH::BodyID id = joltBody->GetID();
        bodyInterface.AddBody(id, JPH::EActivation::Activate);
        return id.GetIndexAndSequenceNumber();
    }

    void PhysicsWorld::DestroyBody(uint32_t bodyId)
    {
        if (!m_initialised || bodyId == INVALID_PHYSICS_BODY)
        {
            return;
        }

        JPH::BodyInterface& bodyInterface = m_impl->PhysSystem->GetBodyInterface();
        const JPH::BodyID joltId(bodyId);
        bodyInterface.RemoveBody(joltId);
        bodyInterface.DestroyBody(joltId);
    }

    Float3 PhysicsWorld::GetBodyPosition(uint32_t bodyId) const
    {
        if (!m_initialised || bodyId == INVALID_PHYSICS_BODY)
        {
            return {0.0f, 0.0f, 0.0f};
        }

        const JPH::BodyInterface& bodyInterface = m_impl->PhysSystem->GetBodyInterface();
        const JPH::RVec3 pos = bodyInterface.GetPosition(JPH::BodyID(bodyId));
        return {pos.GetX(), pos.GetY(), pos.GetZ()};
    }

    Float4 PhysicsWorld::GetBodyRotation(uint32_t bodyId) const
    {
        if (!m_initialised || bodyId == INVALID_PHYSICS_BODY)
        {
            return {0.0f, 0.0f, 0.0f, 1.0f};
        }

        const JPH::BodyInterface& bodyInterface = m_impl->PhysSystem->GetBodyInterface();
        const JPH::Quat rot = bodyInterface.GetRotation(JPH::BodyID(bodyId));
        return {rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW()};
    }

    void PhysicsWorld::SetBodyPosition(uint32_t bodyId, const Float3& position)
    {
        if (!m_initialised || bodyId == INVALID_PHYSICS_BODY)
        {
            return;
        }

        JPH::BodyInterface& bodyInterface = m_impl->PhysSystem->GetBodyInterface();
        bodyInterface.SetPosition(JPH::BodyID(bodyId), ToJoltRVec3(position), JPH::EActivation::Activate);
    }

} // namespace Wayfinder::Physics
