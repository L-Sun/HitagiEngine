module;
#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/ConstraintSolver/btGeneric6DofSpringConstraint.h>

export module interop.bullet;
export namespace hitagi::interop::bullet {
using ::btVector3;
using ::btQuaternion;
using ::btTransform;
using ::btCollisionShape;
using ::btSphereShape;
using ::btBoxShape;
using ::btCapsuleShape;
using ::btDefaultCollisionConfiguration;
using ::btCollisionDispatcher;
using ::btDbvtBroadphase;
using ::btSequentialImpulseConstraintSolver;
using ::btDiscreteDynamicsWorld;
using ::btDefaultMotionState;
using ::btRigidBody;
using ::btGeneric6DofSpringConstraint;
using ::btCollisionObject;
inline constexpr int disable_deactivation = DISABLE_DEACTIVATION;
inline constexpr int stop_erp = BT_CONSTRAINT_STOP_ERP;
}
