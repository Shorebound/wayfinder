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

    // ── Jolt callback implementations ───────────────────────────

    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl()
        {
            m_objectToBroadPhase[PhysicsLayers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            m_objectToBroadPhase[PhysicsLayers::MOVING] = BroadPhaseLayers::MOVING;
        }

        JPH::uint GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            JPH_ASSERT(inLayer < PhysicsLayers::NUM_LAYERS);
            if (inLayer >= PhysicsLayers::NUM_LAYERS) return BroadPhaseLayers::NON_MOVING;
            return m_objectToBroadPhase[inLayer];
        }

        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
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
        JPH::BroadPhaseLayer m_objectToBroadPhase[PhysicsLayers::NUM_LAYERS];
    };

    class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
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
        bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override
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

    // ── Jolt global initialisation guard ────────────────────────

    namespace
    {
        std::once_flag g_joltInitFlag;

        void InitialiseJoltGlobals()
        {
            std::call_once(g_joltInitFlag,
                []()
                {
                    JPH::RegisterDefaultAllocator();
                    JPH::Factory::sInstance = new JPH::Factory();
                    JPH::RegisterTypes();

                    // Register a one-time teardown so Jolt globals are cleaned up
                    // when the process exits (avoids leak reports in sanitisers).
                    std::atexit(
                        []()
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
        ObjectVsBroadPhaseLayerFilterImpl ObjVsBPFilter;
        ObjectLayerPairFilterImpl ObjPairFilter;
    };

    // ── PhysicsWorld public API ─────────────────────────────────

    PhysicsWorld::PhysicsWorld() = default;
    PhysicsWorld::~PhysicsWorld()
    {
        if (m_initialised) Shutdown();
    }

    void PhysicsWorld::Initialise()
    {
        if (m_initialised) return;

        InitialiseJoltGlobals();

        m_impl = std::make_unique<Impl>();
        m_impl->TempAlloc = std::make_unique<JPH::TempAllocatorImpl>(Impl::TEMP_ALLOCATOR_SIZE);
        m_impl->JobSys = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);

        m_impl->PhysSystem = std::make_unique<JPH::PhysicsSystem>();
        m_impl->PhysSystem->Init(
            Impl::MAX_BODIES, Impl::NUM_BODY_MUTEXES, Impl::MAX_BODY_PAIRS, Impl::MAX_CONTACT_CONSTRAINTS, m_impl->BroadPhaseLayerIface, m_impl->ObjVsBPFilter, m_impl->ObjPairFilter);

        m_accumulator = 0.0f;
        m_initialised = true;
        WAYFINDER_INFO(LogPhysics, "PhysicsWorld initialised (Jolt, fixed dt={:.4f}s)", m_fixedTimestep);
    }

    void PhysicsWorld::Shutdown()
    {
        if (!m_initialised) return;

        m_impl.reset();
        m_initialised = false;
        WAYFINDER_INFO(LogPhysics, "PhysicsWorld shut down");
    }

    void PhysicsWorld::Step(float deltaTime)
    {
        if (!m_initialised || deltaTime <= 0.0f) return;

        constexpr int COLLISION_STEPS = 1;
        m_impl->PhysSystem->Update(deltaTime, COLLISION_STEPS, m_impl->TempAlloc.get(), m_impl->JobSys.get());
    }

    int PhysicsWorld::StepFixed(float frameDeltaTime)
    {
        if (!m_initialised || frameDeltaTime <= 0.0f) return 0;

        m_accumulator += frameDeltaTime;

        // Cap the accumulator to avoid a spiral-of-death when a frame takes
        // much longer than expected (e.g. debugger pause, long hitch).
        constexpr float MAX_ACCUMULATED = 0.25f;
        if (m_accumulator > MAX_ACCUMULATED) m_accumulator = MAX_ACCUMULATED;

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

    uint32_t PhysicsWorld::CreateBody(const PhysicsBodyDescriptor& desc, const Float3& position, const Float3& rotationDegrees)
    {
        if (!m_initialised) return INVALID_PHYSICS_BODY;

        // Build shape
        JPH::Ref<JPH::Shape> shape;
        switch (desc.Shape)
        {
        case ColliderShape::Box:
            shape = new JPH::BoxShape(JPH::Vec3(desc.HalfExtents.x, desc.HalfExtents.y, desc.HalfExtents.z));
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
        // TransformComponent.Rotation stores degrees in the same convention
        // as Maths::ComposeTransform (Z-Y-X extrinsic = X-Y-Z intrinsic).
        float rx = Maths::ToRadians(rotationDegrees.x);
        float ry = Maths::ToRadians(rotationDegrees.y);
        float rz = Maths::ToRadians(rotationDegrees.z);
        JPH::Quat rotation = JPH::Quat::sEulerAngles(JPH::Vec3(rx, ry, rz));

        JPH::BodyCreationSettings settings(shape, JPH::RVec3(position.x, position.y, position.z), rotation, motionType, objectLayer);

        // Material properties apply to both dynamic and kinematic bodies.
        settings.mFriction = desc.Friction;
        settings.mRestitution = desc.Restitution;

        if (desc.Type == BodyType::Dynamic)
        {
            settings.mGravityFactor = desc.GravityFactor;
            settings.mLinearDamping = desc.LinearDamping;
            settings.mAngularDamping = desc.AngularDamping;
            settings.mLinearVelocity = JPH::Vec3(desc.LinearVelocity.x, desc.LinearVelocity.y, desc.LinearVelocity.z);
            settings.mAngularVelocity = JPH::Vec3(desc.AngularVelocity.x, desc.AngularVelocity.y, desc.AngularVelocity.z);

            // Override mass if a positive value was provided.
            // Mass <= 0 means "use shape-computed mass".
            if (desc.Mass > 0.0f)
            {
                settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                settings.mMassPropertiesOverride.mMass = desc.Mass;
            }
        }

        JPH::BodyInterface& bodyInterface = m_impl->PhysSystem->GetBodyInterface();
        JPH::Body* joltBody = bodyInterface.CreateBody(settings);
        if (!joltBody)
        {
            WAYFINDER_ERROR(
                LogPhysics, "Failed to create Jolt body (type={}, shape={}, pos=[{},{},{}])", static_cast<int>(desc.Type), static_cast<int>(desc.Shape), position.x, position.y, position.z);
            return INVALID_PHYSICS_BODY;
        }

        JPH::BodyID id = joltBody->GetID();
        bodyInterface.AddBody(id, JPH::EActivation::Activate);
        return id.GetIndexAndSequenceNumber();
    }

    void PhysicsWorld::DestroyBody(uint32_t bodyId)
    {
        if (!m_initialised || bodyId == INVALID_PHYSICS_BODY) return;

        JPH::BodyInterface& bodyInterface = m_impl->PhysSystem->GetBodyInterface();
        JPH::BodyID joltId(bodyId);
        bodyInterface.RemoveBody(joltId);
        bodyInterface.DestroyBody(joltId);
    }

    Float3 PhysicsWorld::GetBodyPosition(uint32_t bodyId) const
    {
        if (!m_initialised || bodyId == INVALID_PHYSICS_BODY) return {0.0f, 0.0f, 0.0f};

        const JPH::BodyInterface& bodyInterface = m_impl->PhysSystem->GetBodyInterface();
        JPH::RVec3 pos = bodyInterface.GetPosition(JPH::BodyID(bodyId));
        return {static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ())};
    }

    Float4 PhysicsWorld::GetBodyRotation(uint32_t bodyId) const
    {
        if (!m_initialised || bodyId == INVALID_PHYSICS_BODY) return {0.0f, 0.0f, 0.0f, 1.0f};

        const JPH::BodyInterface& bodyInterface = m_impl->PhysSystem->GetBodyInterface();
        JPH::Quat rot = bodyInterface.GetRotation(JPH::BodyID(bodyId));
        return {rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW()};
    }

    void PhysicsWorld::SetBodyPosition(uint32_t bodyId, const Float3& position)
    {
        if (!m_initialised || bodyId == INVALID_PHYSICS_BODY) return;

        JPH::BodyInterface& bodyInterface = m_impl->PhysSystem->GetBodyInterface();
        bodyInterface.SetPosition(JPH::BodyID(bodyId), JPH::RVec3(position.x, position.y, position.z), JPH::EActivation::Activate);
    }

} // namespace Wayfinder::Physics
