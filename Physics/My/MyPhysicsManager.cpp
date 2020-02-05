#include <iostream>
#include "MyPhysicsManager.hpp"
#include "Box.hpp"
#include "Plane.hpp"
#include "Sphere.hpp"
#include "RigidBody.hpp"
#include "GraphicsManager.hpp"

using namespace My;

int MyPhysicsManager::Initialize() {
    std::cout << "[MyPhysicsManager] Initialize" << std::endl;
    return 0;
}

void MyPhysicsManager::Finalize() {
    std::cout << "[MyPhysicsManager] Finalize" << std::endl;
    // Clean up
    ClearRigidBodies();
}

void MyPhysicsManager::Tick() {
    if (g_pSceneManager->IsSceneChanged()) {
        ClearRigidBodies();
        CreateRigidBodies();
        g_pSceneManager->NotifySceneIsPhysicalSimulationQueued();
    }
}
void MyPhysicsManager::CreateRigidBody(SceneGeometryNode&         node,
                                       const SceneObjectGeometry& geometry) {
    const float*               param     = geometry.CollisionParameters();
    std::shared_ptr<RigidBody> rigidBody = nullptr;

    switch (geometry.CollisionType()) {
        case SceneObjectCollisionType::kSPHERE: {
            auto       collision_box = std::make_shared<Sphere>(param[0]);
            const auto trans         = node.GetCalculatedTransform();
            auto       motionState   = std::make_shared<MotionState>(*trans);
            rigidBody = std::make_shared<RigidBody>(collision_box, motionState);
        } break;
        case SceneObjectCollisionType::kBOX: {
            auto       collision_box = std::make_shared<Box>(vec3(param));
            const auto trans         = node.GetCalculatedTransform();
            auto       motionState   = std::make_shared<MotionState>(*trans);
            rigidBody = std::make_shared<RigidBody>(collision_box, motionState);
        } break;
        case SceneObjectCollisionType::kPLANE: {
            auto collision_box = std::make_shared<Plane>(vec3(param), param[3]);
            const auto trans   = node.GetCalculatedTransform();
            auto       motionState = std::make_shared<MotionState>(*trans);
            rigidBody = std::make_shared<RigidBody>(collision_box, motionState);
        } break;
        default: {
            // create AABB box according to Bounding Box
            auto bounding_box  = geometry.GetBoundingBox();
            auto collision_box = std::make_shared<Box>(bounding_box.extent);
            const auto trans   = node.GetCalculatedTransform();
            auto       motionState =
                std::make_shared<MotionState>(*trans, bounding_box.centroid);
            rigidBody = std::make_shared<RigidBody>(collision_box, motionState);
        }
    }
    node.LinkRigidBody(rigidBody);
}

void MyPhysicsManager::UpdateRigidBodyTransform(SceneGeometryNode& node) {
    const auto trans     = node.GetCalculatedTransform();
    auto       rigidBody = node.RigidBody();
    auto       motionState =
        std::static_pointer_cast<RigidBody>(rigidBody)->GetMotionState();
    motionState->SetTransition(*trans);
}

void MyPhysicsManager::DeleteRigidBody(SceneGeometryNode& node) {
    node.UnlinkRigidBody();
}
int MyPhysicsManager::CreateRigidBodies() {
    auto& scene = g_pSceneManager->GetSceneForPhysicsSimulation();
    // Geometries

    for (auto [key, pNode] : scene.GeometryNodes) {
        auto pGeometryNode = pNode;
        auto pGeometry = scene.GetGeometry(pGeometryNode->GetSceneObjectRef());
        assert(pGeometry);
        CreateRigidBody(*pGeometryNode, *pGeometry);
    }
    return 0;
}

void MyPhysicsManager::ClearRigidBodies() {
    auto& scene = g_pSceneManager->GetSceneForPhysicsSimulation();
    // Geometries
    for (auto [key, pNode] : scene.GeometryNodes) {
        auto pGeometryNode = pNode;
        DeleteRigidBody(*pGeometryNode);
    }
}

mat4 MyPhysicsManager::GetRigidBodyTransform(void* rigidBody) {
    mat4       trans;
    RigidBody* _rigidBody  = reinterpret_cast<RigidBody*>(rigidBody);
    auto       motionState = _rigidBody->GetMotionState();
    trans                  = motionState->GetTransition();
    return trans;
}

void MyPhysicsManager::ApplyCentralForce(void* rigidBody, vec3 force) {}

#ifdef DEBUG
void MyPhysicsManager::DrawDebugInfo() {
    auto& scene = g_pSceneManager->GetSceneForPhysicsSimulation();

    // Geometries

    for (auto [key, pNode] : scene.GeometryNodes) {
        auto pGeometryNode = pNode;

        if (auto rigidBody =
                std::static_pointer_cast<RigidBody>(pNode->RigidBody())) {
            auto motionState  = rigidBody->GetMotionState();
            auto pGeometry    = rigidBody->GetCollisionShape();
            auto trans        = motionState->GetTransition();
            auto centerOfMass = motionState->GetCenterOfMassOffset();
            DrawAabb(*pGeometry, trans, centerOfMass);
        }
    }
}

void MyPhysicsManager::DrawAabb(const Geometry& geometry, const mat4& trans,
                                const vec3& centerOfMass) {
    vec3 bbMin, bbMax;
    vec3 color(0.7f, 0.6f, 0.5f);
    mat4 _trans(1.0f);
    _trans.data[3][0] = centerOfMass.x * trans.data[0][0];  // scale by x-scale
    _trans.data[3][1] = centerOfMass.y * trans.data[1][1];  // scale by y-scale
    _trans.data[3][2] = centerOfMass.z * trans.data[2][2];  // scale by z-scale
    _trans            = trans * _trans;
    geometry.GetAabb(_trans, bbMin, bbMax);
    g_pGraphicsManager->DrawBox(bbMin, bbMax, color);
}

#endif