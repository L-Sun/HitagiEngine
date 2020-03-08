#include <iostream>
#include "HitagiTest.hpp"
#include "SceneManager.hpp"
#include "DebugManager.hpp"
#include <chrono>
#include <thread>
#include "HitagiPhysicsManager.hpp"

using namespace Hitagi;

int HitagiTest::Initialize() {
    int result;

    std::cout << "Hitagi Game Logic Initialize" << std::endl;
    std::cout << "Start Loading Game Scene" << std::endl;
    result = g_SceneManager->LoadScene("Asset/Scene/test.fbx");
    m_Clock.Initialize();
    m_Clock.Start();
    return result;
}

void HitagiTest::Finalize() { std::cout << "HitagiTest Game Logic Finalize" << std::endl; }

void HitagiTest::Tick() {}

void HitagiTest::OnLeftKey() {
    auto node_weak_ptr = g_SceneManager->GetSceneGeometryNode(selectedNode[i]);

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->Move(-1.0f, 0.0f, 0.0f);
        g_hysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}
void HitagiTest::OnRightKey() {
    auto node_weak_ptr = g_SceneManager->GetSceneGeometryNode(selectedNode[i]);

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->Move(1.0f, 0.0f, 0.0f);
        g_hysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}
void HitagiTest::OnUpKey() {
    auto node_weak_ptr = g_SceneManager->GetSceneGeometryNode(selectedNode[i]);

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->Move(0.0f, 0.0f, 1.0f);
        g_hysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}
void HitagiTest::OnDownKey() {
    auto node_weak_ptr = g_SceneManager->GetSceneGeometryNode(selectedNode[i]);

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->Move(0.0f, 0.0f, -1.0f);
        g_hysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}

void HitagiTest::OnCKey() { i = (i + 1) % selectedNode.size(); }
