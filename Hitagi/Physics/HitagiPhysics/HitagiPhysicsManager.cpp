#include "HitagiPhysicsManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Hitagi::Physics {

int HitagiPhysicsManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("HitagiPhysicsManager");
    m_Logger->info("Initialize.");
    return 0;
}

void HitagiPhysicsManager::Finalize() {
    // Clean up

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void HitagiPhysicsManager::Tick() {}

std::array<vec3f, 2> HitagiPhysicsManager::GetAABB(Resource::SceneGeometryNode& node) {
    auto geometry = node.GetSceneObjectRef().lock();
    if (!geometry) return {vec3f(0), vec3f(0)};

    vec3f aabbMin = vec3f(std::numeric_limits<float>::max());
    vec3f aabbMax = vec3f(std::numeric_limits<float>::min());

    // TODO mesh lod
    for (auto&& mesh : geometry->GetMeshes()) {
        auto& positions    = mesh->GetVertexPropertyArray("POSITION");
        auto  dataType     = positions.GetDataType();
        auto  vertex_count = positions.GetVertexCount();
        auto  data         = positions.GetData();

        switch (dataType) {
            case Resource::VertexDataType::FLOAT3: {
                auto vertex = reinterpret_cast<const vec3f*>(data);
                for (auto i = 0; i < vertex_count; i++, vertex++) {
                    aabbMin = Min(aabbMin, vertex[i]);
                    aabbMax = Max(aabbMax, vertex[i]);
                }
            } break;
            case Resource::VertexDataType::DOUBLE3: {
                auto vertex = reinterpret_cast<const vec3d*>(data);
                for (auto i = 0; i < vertex_count; i++, vertex++) {
                    aabbMin.x = std::min(static_cast<double>(aabbMin.x), vertex->x);
                    aabbMin.y = std::min(static_cast<double>(aabbMin.y), vertex->y);
                    aabbMin.z = std::min(static_cast<double>(aabbMin.z), vertex->z);
                    aabbMax.x = std::max(static_cast<double>(aabbMax.x), vertex->x);
                    aabbMax.y = std::max(static_cast<double>(aabbMax.y), vertex->y);
                    aabbMax.z = std::max(static_cast<double>(aabbMax.z), vertex->z);
                }
            } break;
            default:
                assert(0);
        }
    }

    mat4f trans = node.GetCalculatedTransform();
    // recalculate aabbx after transform
    std::array<vec4f, 8> points = {
        vec4f(aabbMin, 1),
        vec4f(aabbMin.x, aabbMin.y, aabbMax.z, 1),
        vec4f(aabbMin.x, aabbMax.y, aabbMax.z, 1),
        vec4f(aabbMin.x, aabbMax.y, aabbMin.z, 1),
        vec4f(aabbMax, 1),
        vec4f(aabbMax.x, aabbMin.y, aabbMax.z, 1),
        vec4f(aabbMax.x, aabbMax.y, aabbMax.z, 1),
        vec4f(aabbMax.x, aabbMax.y, aabbMin.z, 1),
    };
    for (auto&& p : points) p = trans * p;

    vec3f newAabbMin = vec3f(std::numeric_limits<float>::max()), newAabbMax = vec3f(std::numeric_limits<float>::min());
    for (auto&& p : points) {
        newAabbMin = Min(newAabbMin, vec3f(p.xyz));
        newAabbMax = Max(newAabbMax, vec3f(p.xyz));
    }

    return {std::move(newAabbMin), std::move(newAabbMax)};
}

void HitagiPhysicsManager::CreateRigidBody(Resource::SceneGeometryNode& node) {
    // auto geometry = node.GetSceneObjectRef().lock();
    // if (!geometry) return;

    // const float*               param     = geometry->CollisionParameters();
    // std::shared_ptr<RigidBody> rigidBody = nullptr;

    // switch (geometry->CollisionType()) {
    //     case Resource::SceneObjectCollisionType::SPHERE: {
    //         auto collisionBox = std::make_shared<Sphere>(param[0]);
    //         auto motionState  = std::make_shared<MotionState>(node.GetCalculatedTransform());
    //         rigidBody         = std::make_shared<RigidBody>(collisionBox, motionState);
    //     } break;
    //     case Resource::SceneObjectCollisionType::BOX: {
    //         auto collisionBox = std::make_shared<Box>(vec3f(param));
    //         auto motionState  = std::make_shared<MotionState>(node.GetCalculatedTransform());
    //         rigidBody         = std::make_shared<RigidBody>(collisionBox, motionState);
    //     } break;
    //     case Resource::SceneObjectCollisionType::PLANE: {
    //         auto collisionBox = std::make_shared<Plane>(vec3f(param), param[3]);
    //         auto motionState  = std::make_shared<MotionState>(node.GetCalculatedTransform());
    //         rigidBody         = std::make_shared<RigidBody>(collisionBox, motionState);
    //     } break;
    //     default: {
    //         // create AABB box according to Bounding Box
    //         vec3f aabbMin, aabbMax;
    //         auto  collisionBox = std::make_shared<Box>(aabbMax - aabbMin);
    //         auto  motionState  = std::make_shared<MotionState>(node.GetCalculatedTransform(), 0.5 * (aabbMin + aabbMax));
    //         rigidBody          = std::make_shared<RigidBody>(collisionBox, motionState);
    //     }
    // }
}

void HitagiPhysicsManager::UpdateRigidBodyTransform(Resource::SceneGeometryNode& node) {
    // auto rigidBody   = node.RigidBody();
    // auto motionState = std::static_pointer_cast<RigidBody>(rigidBody)->GetMotionState();
    // motionState->SetTransition(node.GetCalculatedTransform());
}

void HitagiPhysicsManager::DeleteRigidBody(Resource::SceneGeometryNode& node) {}

mat4f HitagiPhysicsManager::GetRigidBodyTransform(Resource::SceneGeometryNode& node) {
    mat4f trans;
    // auto  _rigidBody  = std::static_pointer_cast<RigidBody>(rigidBody);
    // auto  motionState = _rigidBody->GetMotionState();
    // trans             = motionState->GetTransition();
    return trans;
}

void HitagiPhysicsManager::ApplyCentralForce(Resource::SceneGeometryNode& node, vec3f force) {}

#if defined(_DEBUG)
void HitagiPhysicsManager::DrawDebugInfo() {
    // Geometries

    // for (auto [key, node] : scene.GeometryNodes) {
    //     auto pGeometryNode = node;

    //     if (auto rigidBody = std::static_pointer_cast<RigidBody>(node->RigidBody())) {
    //         auto motionState  = rigidBody->GetMotionState();
    //         auto pGeometry    = rigidBody->GetCollisionShape();
    //         auto trans        = motionState->GetTransition();
    //         auto centerOfMass = motionState->GetCenterOfMassOffset();
    //         DrawAabb(*pGeometry, trans, centerOfMass);
    //     }
    // }
}

void HitagiPhysicsManager::DrawAabb(const Geometry& geometry, const mat4f& trans, const vec3f& centerOfMass) {}

#endif
}  // namespace Hitagi::Physics