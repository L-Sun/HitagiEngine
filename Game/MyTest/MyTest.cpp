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
    result = g_pSceneManager->LoadScene("Scene/cube.ogex");

    return result;
}

void MyTest::Finalize() { cout << "MyTest Game Logic Finalize" << endl; }

void MyTest::Tick() { this_thread::sleep_for(std::chrono::milliseconds(60)); }

void MyTest::OnLeftKey() {
    auto node_weak_ptr = g_pSceneManager->GetSceneGeometryNode("Suzanne");

    if (auto node_ptr = node_weak_ptr.lock()) {
        node_ptr->RotateBy(-PI / 6.0f, 0.0f, 0.0f);
        g_pPhysicsManager->UpdateRigidBodyTransform(*node_ptr);
    }
}

void MyTest::OnDKey() {}
