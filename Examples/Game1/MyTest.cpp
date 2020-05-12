#include "MyTest.hpp"
#include "D3D12GraphicsManager.hpp"

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
    g_InputManager->Map(RAY_TRACING, InputEvent::KEY_SPACE);

    return result;
}

void MyTest::Finalize() { GameLogic::Finalize(); }

void MyTest::Tick() {
    if (g_InputManager->GetBoolNew(DEBUG_TOGGLE)) {
        g_DebugManager->ToggleDebugInfo();
    }
    if (auto z = g_InputManager->GetFloat(ZOOM)) {
        if (auto camera = g_SceneManager->GetCameraNode().lock()) {
            auto direct = camera->GetCameraLookAt();
            camera->ApplyTransform(translate(mat4f(1.0f), direct * 10 * z));
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

    LightMove();
    if (g_InputManager->GetBoolNew(RAY_TRACING)) {
        static_cast<Graphics::DX12::D3D12GraphicsManager*>(g_GraphicsManager.get())->ToggleRayTrancing();
    }

    if (g_InputManager->GetBool(RESET_SCENE)) {
        g_SceneManager->SetScene("Asset/Scene/test.fbx");
        g_SceneManager->ResetScene();
    }
}

void MyTest::LightMove() {
    if (auto light = g_SceneManager->GetSceneLightNode("Point").lock()) {
        vec3f moveDistance(0);
        bool  updated = false;
        if (g_InputManager->GetBool(MOVE_LEFT)) moveDistance.x += 0.05;
        if (g_InputManager->GetBool(MOVE_RIGHT)) moveDistance.x -= 0.05;
        if (g_InputManager->GetBool(MOVE_UP)) moveDistance.z += 0.05;
        if (g_InputManager->GetBool(MOVE_DOWN)) moveDistance.z -= 0.05;
        if (g_InputManager->GetBool(MOVE_FRONT)) moveDistance.y += 0.05;
        if (g_InputManager->GetBool(MOVE_BACK)) moveDistance.y -= 0.05;
        if (moveDistance.norm() != 0)
            light->ApplyTransform(translate(mat4f(1.0f), moveDistance));
    }
}