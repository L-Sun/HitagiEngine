#include <iostream>
#include "MyTest.hpp"
#include "SceneManager.hpp"
#include "DebugManager.hpp"
#include <chrono>
#include <thread>
#include "My/MyPhysicsManager.hpp"

using namespace std;
using namespace My;

int MyTest::Initialize() {
    int result;

    cout << "My Game Logic Initialize" << endl;
    cout << "Start Loading Game Scene" << endl;
    result = g_pSceneManager->LoadScene("Scene/balls.ogex");
    m_clock.Initialize();
    m_clock.Start();
    return result;
}

void MyTest::Finalize() { cout << "MyTest Game Logic Finalize" << endl; }

void MyTest::Tick() {}

void MyTest::OnLeftKey() {
    auto node_weak_ptr = g_pSceneManager->GetSceneGeometryNode(selectedNode[i]);

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->Move(-1.0f, 0.0f, 0.0f);
        g_pPhysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}
void MyTest::OnRightKey() {
    auto node_weak_ptr = g_pSceneManager->GetSceneGeometryNode(selectedNode[i]);

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->Move(1.0f, 0.0f, 0.0f);
        g_pPhysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}
void MyTest::OnUpKey() {
    auto node_weak_ptr = g_pSceneManager->GetSceneGeometryNode(selectedNode[i]);

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->Move(0.0f, 0.0f, 1.0f);
        g_pPhysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}
void MyTest::OnDownKey() {
    auto node_weak_ptr = g_pSceneManager->GetSceneGeometryNode(selectedNode[i]);

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->Move(0.0f, 0.0f, -1.0f);
        g_pPhysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}

void MyTest::OnCKey() { i = (i + 1) % selectedNode.size(); }
