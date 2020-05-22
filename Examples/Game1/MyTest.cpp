#include "MyTest.hpp"

using namespace Hitagi;

int MyTest::Initialize() {
    int result = GameLogic::Initialize();
    g_SceneManager->SetScene("Asset/Scene/test.fbx");
    g_InputManager->Map(DEBUG_TOGGLE, InputEvent::KEY_D);
    g_InputManager->Map(ZOOM, InputEvent::MOUSE_SCROLL_Y);
    g_InputManager->Map(ROTATE_ON, InputEvent::MOUSE_MIDDLE);
    g_InputManager->Map(ROTATE_Z, InputEvent::MOUSE_MOVE_X);
    g_InputManager->Map(ROTATE_H, InputEvent::MOUSE_MOVE_Y);

    g_InputManager->Map(MOVE_UP, InputEvent::KEY_UP);
    g_InputManager->Map(MOVE_DOWN, InputEvent::KEY_DOWN);
    g_InputManager->Map(MOVE_LEFT, InputEvent::KEY_LEFT);
    g_InputManager->Map(MOVE_RIGHT, InputEvent::KEY_RIGHT);
    g_InputManager->Map(MOVE_FRONT, InputEvent::KEY_W);
    g_InputManager->Map(MOVE_BACK, InputEvent::KEY_S);

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

    if (auto z = g_InputManager->GetFloat(ZOOM)) {
        if (auto camera = g_SceneManager->GetCameraNode().lock()) {
            auto direct = camera->GetCameraLookAt();
            camera->ApplyTransform(translate(mat4f(1.0f), direct * 1 * z));
        }
    }
    if (g_InputManager->GetBool(ROTATE_ON)) {
        if (auto angleH = -g_InputManager->GetFloatDelta(ROTATE_H) * sensitivity; angleH != 0) {
            if (auto camera = g_SceneManager->GetCameraNode().lock()) {
                auto direct = camera->GetCameraRight();
                camera->ApplyTransform(rotate(mat4f(1.0f), radians(angleH), direct));
            }
        }
        if (auto angleZ = -g_InputManager->GetFloatDelta(ROTATE_Z) * sensitivity; angleZ != 0) {
            if (auto camera = g_SceneManager->GetCameraNode().lock()) {
                int sign = (camera->GetCameraUp().z > 0) ? 1 : -1;
                camera->ApplyTransform(rotateZ(mat4f(1.0f), radians(sign * angleZ)));
            }
        }
    }

    if (g_InputManager->GetBoolNew(RESET_SCENE)) {
        g_SceneManager->SetScene("Asset/Scene/test.fbx");
        g_SceneManager->ResetScene();
    }
}
