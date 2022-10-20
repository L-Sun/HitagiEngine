#include <hitagi/physics/hitagi_physics.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace hitagi::math;

namespace hitagi::physics {

bool hitagiPhysicsManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("HitagiPhysicsManager");
    m_Logger->info("Initialize.");
    return true;
}

void hitagiPhysicsManager::Finalize() {
    // Clean up

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

std::array<vec3f, 2> hitagiPhysicsManager::GetAABB(asset::Geometry& node) {
    auto geometry = node.GetResourceObjectRef().lock();
    if (!geometry) return {vec3f(0), vec3f(0)};

    vec3f aabb_min = vec3f(std::numeric_limits<float>::max());
    vec3f aabb_max = vec3f(std::numeric_limits<float>::min());

    // TODO mesh lod
    for (auto&& mesh : geometry->GetMeshes()) {
        auto& positions    = mesh.GetVertexByName("POSITION");
        auto  data_type    = positions.GetDataType();
        auto  vertex_count = positions.GetVertexCount();
        auto  data         = positions.GetData();

        switch (data_type) {
            case asset::VertexArray::DataType::Float3: {
                auto vertex = reinterpret_cast<const vec3f*>(data);
                for (auto i = 0; i < vertex_count; i++, vertex++) {
                    aabb_min = min(aabb_min, vertex[i]);
                    aabb_max = max(aabb_max, vertex[i]);
                }
            } break;
            case asset::VertexArray::DataType::Double3: {
                auto vertex = reinterpret_cast<const vec3d*>(data);
                for (auto i = 0; i < vertex_count; i++, vertex++) {
                    aabb_min.x = std::min(static_cast<double>(aabb_min.x), vertex->x);
                    aabb_min.y = std::min(static_cast<double>(aabb_min.y), vertex->y);
                    aabb_min.z = std::min(static_cast<double>(aabb_min.z), vertex->z);
                    aabb_max.x = std::max(static_cast<double>(aabb_max.x), vertex->x);
                    aabb_max.y = std::max(static_cast<double>(aabb_max.y), vertex->y);
                    aabb_max.z = std::max(static_cast<double>(aabb_max.z), vertex->z);
                }
            } break;
            default:
                assert(0);
        }
    }

    mat4f trans = node.GetCalculatedTransformation();
    // recalculate aabbx after transform
    std::array<vec4f, 8> points = {
        vec4f(aabb_min, 1),
        vec4f(aabb_min.x, aabb_min.y, aabb_max.z, 1),
        vec4f(aabb_min.x, aabb_max.y, aabb_max.z, 1),
        vec4f(aabb_min.x, aabb_max.y, aabb_min.z, 1),
        vec4f(aabb_max, 1),
        vec4f(aabb_max.x, aabb_min.y, aabb_max.z, 1),
        vec4f(aabb_max.x, aabb_max.y, aabb_max.z, 1),
        vec4f(aabb_max.x, aabb_max.y, aabb_min.z, 1),
    };
    for (auto&& p : points) p = trans * p;

    vec3f new_aabb_min = vec3f(std::numeric_limits<float>::max()), new_aabb_max = vec3f(std::numeric_limits<float>::min());
    for (auto&& p : points) {
        new_aabb_min = min(new_aabb_min, vec3f(p.xyz));
        new_aabb_max = max(new_aabb_max, vec3f(p.xyz));
    }

    return {std::move(new_aabb_min), std::move(new_aabb_max)};
}

void hitagiPhysicsManager::CreateRigidBody(asset::Geometry& node) {
    // auto geometry = node.GetResourceObjectRef().lock();
    // if (!geometry) return;

    // const float*               param     = geometry->CollisionParameters();
    // std::shared_ptr<RigidBody> rigidBody = nullptr;

    // switch (geometry->CollisionType()) {
    //     case asset::ResourceObjectCollisionType::SPHERE: {
    //         auto collisionBox = std::make_shared<Sphere>(param[0]);
    //         auto motionState  = std::make_shared<MotionState>(node.GetCalculatedTransformation());
    //         rigidBody         = std::make_shared<RigidBody>(collisionBox, motionState);
    //     } break;
    //     case asset::ResourceObjectCollisionType::BOX: {
    //         auto collisionBox = std::make_shared<Box>(vec3f(param));
    //         auto motionState  = std::make_shared<MotionState>(node.GetCalculatedTransformation());
    //         rigidBody         = std::make_shared<RigidBody>(collisionBox, motionState);
    //     } break;
    //     case asset::ResourceObjectCollisionType::PLANE: {
    //         auto collisionBox = std::make_shared<Plane>(vec3f(param), param[3]);
    //         auto motionState  = std::make_shared<MotionState>(node.GetCalculatedTransformation());
    //         rigidBody         = std::make_shared<RigidBody>(collisionBox, motionState);
    //     } break;
    //     default: {
    //         // create AABB box according to Bounding Box
    //         vec3f aabbMin, aabbMax;
    //         auto  collisionBox = std::make_shared<Box>(aabbMax - aabbMin);
    //         auto  motionState  = std::make_shared<MotionState>(node.GetCalculatedTransformation(), 0.5 * (aabbMin + aabbMax));
    //         rigidBody          = std::make_shared<RigidBody>(collisionBox, motionState);
    //     }
    // }
}

void hitagiPhysicsManager::UpdateRigidBodyTransform(asset::Geometry& node) {
    // auto rigidBody   = node.RigidBody();
    // auto motionState = std::static_pointer_cast<RigidBody>(rigidBody)->GetMotionState();
    // motionState->SetTransition(node.GetCalculatedTransformation());
}

void hitagiPhysicsManager::DeleteRigidBody(asset::Geometry& node) {}

mat4f hitagiPhysicsManager::GetRigidBodyTransform(asset::Geometry& node) {
    mat4f trans;
    // auto  _rigidBody  = std::static_pointer_cast<RigidBody>(rigidBody);
    // auto  motionState = _rigidBody->GetMotionState();
    // trans             = motionState->GetTransition();
    return trans;
}

void hitagiPhysicsManager::ApplyCentralForce(asset::Geometry& node, vec3f force) {}

}  // namespace hitagi::physics