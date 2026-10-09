module;
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/EPhysicsUpdateError.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

export module interop.jolt;

export namespace hitagi::interop {
// Work around Clang's TU-local exposure diagnostic for constant-value initialization.
inline constexpr int max_physics_barriers = static_cast<int>(JPH::cMaxPhysicsBarriers);
}

export namespace JPH {
using ::JPH::BodyCreationSettings;
using ::JPH::BodyID;
using ::JPH::BoxShapeSettings;
using ::JPH::BroadPhaseLayer;
using ::JPH::BroadPhaseLayerInterface;
using ::JPH::ColorArg;
using ::JPH::EActivation;
using ::JPH::EMotionType;
using ::JPH::Factory;
using ::JPH::JobSystemWithBarrier;
using ::JPH::ObjectLayer;
using ::JPH::ObjectLayerPairFilter;
using ::JPH::ObjectVsBroadPhaseLayerFilter;
using ::JPH::PhysicsSystem;
using ::JPH::Quat;
using ::JPH::QuatArg;
using ::JPH::RefConst;
using ::JPH::RegisterDefaultAllocator;
using ::JPH::RegisterTypes;
using ::JPH::RVec3;
using ::JPH::RVec3Arg;
using ::JPH::Shape;
using ::JPH::SphereShapeSettings;
using ::JPH::TempAllocatorImpl;
using ::JPH::Trace;
using ::JPH::uint;
using ::JPH::uint32;
using ::JPH::UnregisterTypes;
using ::JPH::Vec3;
using ::JPH::Vec3Arg;
}  // namespace JPH
#ifdef JPH_ENABLE_ASSERTS
export namespace JPH {
using ::JPH::AssertFailed;
}
#endif
