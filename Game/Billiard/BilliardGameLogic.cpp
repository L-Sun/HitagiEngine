#include <iostream>
#include "BilliardGameLogic.hpp"
#include "SceneManager.hpp"
#include "Bullet/BulletPhysicsManager.hpp"

using namespace std;
using namespace My;

int BilliardGameLogic::Initialize() {
    int result;

    cout << "Biiliard Game Logic Initialize" << endl;
    cout << "Start Loading Game Scene" << endl;
    result = g_pSceneManager->LoadScene("Scene/billiard.ogex");

    return result;
}

void BilliardGameLogic::Finalize() {
    cout << "Biiliard Game Logic Finalize" << endl;
}

void BilliardGameLogic::Tick() {}

void BilliardGameLogic::OnLeftKey() {
    auto ptr = g_pSceneManager->GetSceneGeometryNode("pbb_cue");
    if (auto node = ptr.lock()) {
        auto rigidBody = node->RigidBody();
        if (rigidBody) {
            g_pPhysicsManager->ApplyCentralForce(rigidBody,
                                                 vec3(-100.0f, 0.0f, 0.0f));
        }
    }
}