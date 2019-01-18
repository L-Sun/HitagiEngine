#include <iostream>
#include "MyPhysicsManager.hpp"
#include "Box.hpp"
#include "Plane.hpp"
#include "Sphere.hpp"
#include "RigidBody.hpp"
#include "GraphicsManager.hpp"

using namespace My;
using namespace std;

int MyPhysicsManager::Initialize() {
    cout << "[MyPhysicsManager] Initialize" << endl;
    return 0;
}

void MyPhysicsManager::Finalize() {
    cout << "[MyPhysicsManager] Finalize" << endl;
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
    const float* param     = geometry.CollisionParameters();
    RigidBody*   rigidBody = nullptr;

    switch (geometry.CollisionType()) {
        case SceneObjectCollisionType::kSPHERE: {
            auto       collision_box = make_shared<Sphere>(param[0]);
            const auto trans         = node.GetCalculatedTransform();
            auto       motionState   = make_shared<MotionState>(*trans);
            rigidBody = new RigidBody(collision_box, motionState);
        } break;
        case SceneObjectCollisionType::kBOX: {
            auto       collision_box = make_shared<Box>(vec3(param));
            const auto trans         = node.GetCalculatedTransform();
            auto       motionState   = make_shared<MotionState>(*trans);
            rigidBody = new RigidBody(collision_box, motionState);
        } break;
        case SceneObjectCollisionType::kPLANE: {
            auto collision_box     = make_shared<Plane>(vec3(param), param[3]);
            const auto trans       = node.GetCalculatedTransform();
            auto       motionState = make_shared<MotionState>(*trans);
            rigidBody              = new RigidBody(collision_box, motionState);
        } break;
        default:
            break;
    }
    node.LinkRigidBody(rigidBody);
}

void MyPhysicsManager::UpdateRigidBodyTransform(SceneGeometryNode& node) {
    const auto trans     = node.GetCalculatedTransform();
    auto       rigidBody = node.RigidBody();
    auto       motionState =
        reinterpret_cast<RigidBody*>(rigidBody)->GetMotionState();
    motionState->SetTransition(*trans);
}

void MyPhysicsManager::DeleteRigidBody(SceneGeometryNode& node) {
    RigidBody* rigidBody = reinterpret_cast<RigidBody*>(node.UnlinkRigidBody());
    if (rigidBody) {
        delete rigidBody;
    }
}

int MyPhysicsManager::CreateRigidBodies() {
    auto& scene = g_pSceneManager->GetSceneForPhysicsSimulation();
    // Geometries

    for (auto _it : scene.GeometryNodes) {
        auto pGeometryNode = _it.second;
        auto pGeometry = scene.GetGeometry(pGeometryNode->GetSceneObjectRef());
        assert(pGeometry);
        CreateRigidBody(*pGeometryNode, *pGeometry);
    }
    return 0;
}

void MyPhysicsManager::ClearRigidBodies() {
    auto& scene = g_pSceneManager->GetSceneForPhysicsSimulation();
    // Geometries
    for (auto _it : scene.GeometryNodes) {
        auto pGeometryNode = _it.second;
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

    for (auto _it : scene.GeometryNodes) {
        auto pGeometryNode = _it.second;

        if (void* rigidBody = pGeometryNode->RigidBody()) {
            RigidBody* _rigidBody = reinterpret_cast<RigidBody*>(rigidBody);
            mat4       simulated_result = GetRigidBodyTransform(_rigidBody);
            auto       pGeometry        = _rigidBody->GetCollisionShape();
            DrawAabb(*pGeometry, simulated_result);
        }
    }
}

void MyPhysicsManager::DrawAabb(const Geometry& geometry, const mat4& trans) {
    vec3 bbMin, bbMax;
    vec3 color(0.5f, 0.5f, 0.5f);

    geometry.GetAabb(trans, bbMin, bbMax);
    g_pGraphicsManager->DrawBox(bbMin, bbMax, color);
}

#endif