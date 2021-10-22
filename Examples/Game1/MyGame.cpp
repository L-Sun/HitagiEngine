#include "MyGame.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace Hitagi;

int MyGame::Initialize() {
    m_Logger = spdlog::stdout_color_mt("MyGame");
    g_SceneManager->SetScene("Asset/Scene/untitled.fbx");
    g_InputManager->Map(DEBUG_TOGGLE, VirtualKeyCode::KEY_SPACE);

    g_InputManager->Map(MOVE_LEFT, VirtualKeyCode::KEY_A);
    g_InputManager->Map(MOVE_RIGHT, VirtualKeyCode::KEY_D);
    g_InputManager->Map(MOVE_FRONT, VirtualKeyCode::KEY_W);
    g_InputManager->Map(MOVE_BACK, VirtualKeyCode::KEY_S);
    g_InputManager->Map(MOVE_UP, VirtualKeyCode::KEY_Z);
    g_InputManager->Map(MOVE_DOWN, VirtualKeyCode::KEY_X);
    g_InputManager->Map(ROTATE_H, MouseEvent::MOVE_X);
    g_InputManager->Map(ROTATE_V, MouseEvent::MOVE_Y);

    g_InputManager->Map(RESET_SCENE, VirtualKeyCode::KEY_R);
    g_InputManager->Map(MSAA, VirtualKeyCode::KEY_M);

    m_Clock.Initialize();
    m_Clock.Start();
    return 0;
}

void MyGame::Finalize() {
    m_Logger->info("MyGame Finalize");
    m_Logger = nullptr;
}

void MyGame::Tick() {
    m_Clock.Tick();
    float deltaTime = m_Clock.deltaTime().count();

    if (g_InputManager->GetBoolNew(DEBUG_TOGGLE)) {
        g_DebugManager->ToggleDebugInfo();
    }

    if (auto camera = g_SceneManager->GetCameraNode().lock()) {
        auto  position = camera->GetCameraPosition();
        auto  front    = camera->GetCameraLookAt();
        auto  right    = camera->GetCameraRight();
        float phi      = 0;
        float theta    = 0;
        camera->ApplyTransform(translate(rotateZ(rotate(translate(mat4f(1.0f), -position), theta, right), phi), position));

        right = camera->GetCameraRight();
        front = camera->GetCameraLookAt();
        // Move in plane, so z is equal to zero
        front.z = 0;
        right.z = 0;
        vec3f       move_vec(0.0f, 0.0f, 0.0f);
        const float speed = 1;
        if (g_InputManager->GetBool(MOVE_LEFT)) move_vec += -right;
        if (g_InputManager->GetBool(MOVE_RIGHT)) move_vec += right;
        if (g_InputManager->GetBool(MOVE_FRONT)) move_vec += front;
        if (g_InputManager->GetBool(MOVE_BACK)) move_vec += -front;
        if (g_InputManager->GetBool(MOVE_UP)) move_vec.z += 1;
        if (g_InputManager->GetBool(MOVE_DOWN)) move_vec.z -= 1;
        camera->ApplyTransform(translate(mat4f(1.0f), speed * deltaTime * move_vec));
    }

    if (g_InputManager->GetBoolNew(RESET_SCENE)) {
        g_SceneManager->SetScene("Asset/Scene/untitled.fbx");
        g_SceneManager->ResetScene();
    }
}
