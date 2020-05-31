#include "MyTest.hpp"

using namespace Hitagi;

int MyTest::Initialize() {
    int result = GameLogic::Initialize();
    g_SceneManager->SetScene("Asset/Scene/spot.fbx");
    g_InputManager->Map(DEBUG_TOGGLE, InputEvent::KEY_SPACE);

    g_InputManager->Map(MOVE_LEFT, InputEvent::KEY_A);
    g_InputManager->Map(MOVE_RIGHT, InputEvent::KEY_D);
    g_InputManager->Map(MOVE_FRONT, InputEvent::KEY_W);
    g_InputManager->Map(MOVE_BACK, InputEvent::KEY_S);
    g_InputManager->Map(MOVE_UP, InputEvent::KEY_Z);
    g_InputManager->Map(MOVE_DOWN, InputEvent::KEY_X);
    g_InputManager->Map(ROTATE_H, InputEvent::MOUSE_MOVE_X);
    g_InputManager->Map(ROTATE_V, InputEvent::MOUSE_MOVE_Y);

    g_InputManager->Map(RESET_SCENE, InputEvent::KEY_R);
    g_InputManager->Map(MSAA, InputEvent::KEY_M);

    return result;
}

void MyTest::Finalize() { GameLogic::Finalize(); }

void MyTest::Tick() {
    if (g_InputManager->GetBoolNew(DEBUG_TOGGLE)) {
        g_DebugManager->ToggleDebugInfo();
    }
    if (g_InputManager->GetBoolNew(MSAA)) {
        g_GraphicsManager->ToogleMSAA();
    }

    if (auto camera = g_SceneManager->GetCameraNode().lock()) {
        auto  position = camera->GetCameraPosition();
        auto  front    = camera->GetCameraLookAt();
        auto  right    = camera->GetCameraRight();
        float phi      = -radians(sensitivity * g_InputManager->GetFloatDelta(ROTATE_H));
        float theta    = -radians(sensitivity * g_InputManager->GetFloatDelta(ROTATE_V));

        camera->ApplyTransform(translate(rotateZ(rotate(translate(mat4f(1.0f), -position), theta, right), phi), position));

        right = camera->GetCameraRight();
        front = camera->GetCameraLookAt();
        // Move in plane, so z is equal to zero
        front.z = 0;
        right.z = 0;
        vec3f move_vec(0.0f, 0.0f, 0.0f);
        if (g_InputManager->GetBool(MOVE_LEFT)) move_vec += -right;
        if (g_InputManager->GetBool(MOVE_RIGHT)) move_vec += right;
        if (g_InputManager->GetBool(MOVE_FRONT)) move_vec += front;
        if (g_InputManager->GetBool(MOVE_BACK)) move_vec += -front;
        if (g_InputManager->GetBool(MOVE_UP)) move_vec.z += 1;
        if (g_InputManager->GetBool(MOVE_DOWN)) move_vec.z -= 1;
        camera->ApplyTransform(translate(mat4f(1.0f), sensitivity * move_vec));
    }

    if (g_InputManager->GetBoolNew(RESET_SCENE)) {
        // g_SceneManager->SetScene("Asset/Scene/spot.fbx");
        g_SceneManager->ResetScene();
    }
}
