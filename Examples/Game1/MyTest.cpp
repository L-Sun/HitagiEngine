#include <iostream>
#include <chrono>
#include <thread>
#include "MyTest.hpp"
#include "SceneManager.hpp"
#include "DebugManager.hpp"
#include "HitagiPhysicsManager.hpp"

using namespace Hitagi;

int MyTest::Initialize() {
    int result;

    std::cout << "Hitagi Game Logic Initialize" << std::endl;
    std::cout << "Start Loading Game Scene" << std::endl;
    result = g_SceneManager->LoadScene("Asset/Scene/test.fbx");
    m_Clock.Initialize();
    m_Clock.Start();
    return result;
}

void MyTest::Finalize() { std::cout << "MyTest Game Logic Finalize" << std::endl; }

void MyTest::Tick() {}

void MyTest::OnLeftKey() {
    auto node_weak_ptr = g_SceneManager->GetSceneGeometryNode(selectedNode[i]);

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->Move(-1.0f, 0.0f, 0.0f);
        g_PhysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}
void MyTest::OnRightKey() {
    auto node_weak_ptr = g_SceneManager->GetSceneGeometryNode(selectedNode[i]);

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->Move(1.0f, 0.0f, 0.0f);
        g_PhysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}
void MyTest::OnUpKey() {
    auto node_weak_ptr = g_SceneManager->GetSceneGeometryNode(selectedNode[i]);

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->Move(0.0f, 0.0f, 1.0f);
        g_PhysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}
void MyTest::OnDownKey() {
    auto node_weak_ptr = g_SceneManager->GetSceneGeometryNode(selectedNode[i]);

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->Move(0.0f, 0.0f, -1.0f);
        g_PhysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}

void MyTest::OnCKey() { i = (i + 1) % selectedNode.size(); }
