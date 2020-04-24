#include "HitagiPhysicsManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "Box.hpp"
#include "Plane.hpp"
#include "Sphere.hpp"
#include "RigidBody.hpp"
#include "GraphicsManager.hpp"

namespace Hitagi::Physics {

int HitagiPhysicsManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("HitagiPhysicsManager");
    m_Logger->info("Initialize.");
    return 0;
}

void HitagiPhysicsManager::Finalize() {
    // Clean up
    ClearRigidBodies();

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void HitagiPhysicsManager::Tick() {
    if (g_SceneManager->IsSceneChanged()) {
        ClearRigidBodies();
        CreateRigidBodies();
        g_SceneManager->NotifySceneIsPhysicalSimulationQueued();
    }
}
void HitagiPhysicsManager::CreateRigidBody(Resource::SceneGeometryNode&         node,
                                           const Resource::SceneObjectGeometry& geometry) {
    const float*               param     = geometry.CollisionParameters();
    std::shared_ptr<RigidBody> rigidBody = nullptr;

    switch (geometry.CollisionType()) {
        case Resource::SceneObjectCollisionType::SPHERE: {
            auto collisionBox = std::make_shared<Sphere>(param[0]);
            auto motionState  = std::make_shared<MotionState>(node.GetCalculatedTransform());
            rigidBody         = std::make_shared<RigidBody>(collisionBox, motionState);
        } break;
        case Resource::SceneObjectCollisionType::BOX: {
            auto collisionBox = std::make_shared<Box>(vec3f(param));
            auto motionState  = std::make_shared<MotionState>(node.GetCalculatedTransform());
            rigidBody         = std::make_shared<RigidBody>(collisionBox, motionState);
        } break;
        case Resource::SceneObjectCollisionType::PLANE: {
            auto collisionBox = std::make_shared<Plane>(vec3f(param), param[3]);
            auto motionState  = std::make_shared<MotionState>(node.GetCalculatedTransform());
            rigidBody         = std::make_shared<RigidBody>(collisionBox, motionState);
        } break;
        default: {
            // create AABB box according to Bounding Box
            auto boundingBox  = geometry.GetBoundingBox();
            auto collisionBox = std::make_shared<Box>(boundingBox.extent);
            auto motionState  = std::make_shared<MotionState>(node.GetCalculatedTransform(), boundingBox.centroid);
            rigidBody         = std::make_shared<RigidBody>(collisionBox, motionState);
        }
    }
    node.LinkRigidBody(rigidBody);
}

void HitagiPhysicsManager::UpdateRigidBodyTransform(Resource::SceneGeometryNode& node) {
    auto rigidBody   = node.RigidBody();
    auto motionState = std::static_pointer_cast<RigidBody>(rigidBody)->GetMotionState();
    motionState->SetTransition(node.GetCalculatedTransform());
}

void HitagiPhysicsManager::DeleteRigidBody(Resource::SceneGeometryNode& node) { node.UnlinkRigidBody(); }
int  HitagiPhysicsManager::CreateRigidBodies() {
    auto& scene = g_SceneManager->GetSceneForPhysicsSimulation();
    // Geometries
    for (auto [key, node] : scene.GeometryNodes) {
        auto pGeometryNode = node;
        auto pGeometry     = scene.GetGeometry(pGeometryNode->GetSceneObjectRef());
        assert(pGeometry);
        CreateRigidBody(*pGeometryNode, *pGeometry);
    }
    return 0;
}

void HitagiPhysicsManager::ClearRigidBodies() {
    auto& scene = g_SceneManager->GetSceneForPhysicsSimulation();
    // Geometries
    for (auto [key, node] : scene.GeometryNodes) {
        auto pGeometryNode = node;
        DeleteRigidBody(*pGeometryNode);
    }
}

mat4f HitagiPhysicsManager::GetRigidBodyTransform(std::shared_ptr<void> rigidBody) {
    mat4f trans;
    auto  _rigidBody  = std::static_pointer_cast<RigidBody>(rigidBody);
    auto  motionState = _rigidBody->GetMotionState();
    trans             = motionState->GetTransition();
    return trans;
}

void HitagiPhysicsManager::ApplyCentralForce(std::shared_ptr<void> rigidBody, vec3f force) {}

#if defined(_DEBUG)
void HitagiPhysicsManager::DrawDebugInfo() {
    auto& scene = g_SceneManager->GetSceneForPhysicsSimulation();

    // Geometries

    for (auto [key, node] : scene.GeometryNodes) {
        auto pGeometryNode = node;

        if (auto rigidBody = std::static_pointer_cast<RigidBody>(node->RigidBody())) {
            auto motionState  = rigidBody->GetMotionState();
            auto pGeometry    = rigidBody->GetCollisionShape();
            auto trans        = motionState->GetTransition();
            auto centerOfMass = motionState->GetCenterOfMassOffset();
            DrawAabb(*pGeometry, trans, centerOfMass);
        }
    }
}

void HitagiPhysicsManager::DrawAabb(const Geometry& geometry, const mat4f& trans, const vec3f& centerOfMass) {
    vec3f bbMin, bbMax;
    vec3f color(0.7f, 0.6f, 0.5f);
    mat4f _trans(1.0f);
    _trans.data[3][0] = centerOfMass.x * trans.data[0][0];  // scale by x-scale
    _trans.data[3][1] = centerOfMass.y * trans.data[1][1];  // scale by y-scale
    _trans.data[3][2] = centerOfMass.z * trans.data[2][2];  // scale by z-scale
    _trans            = trans * _trans;
    geometry.GetAabb(_trans, bbMin, bbMax);
    g_GraphicsManager->RenderBox(bbMin, bbMax, color);
}

#endif
}  // namespace Hitagi::Physics