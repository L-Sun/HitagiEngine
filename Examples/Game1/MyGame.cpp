#include "MyGame.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace Hitagi;

int MyGame::Initialize() {
    m_Logger   = spdlog::stdout_color_mt("MyGame");
    auto scene = g_AssetManager->ImportScene("Asset/Scene/stanford-bunny.fbx");
    g_SceneManager->SetScene(scene);

    g_InputManager->Map("MOVE_LEFT", VirtualKeyCode::KEY_A);
    g_InputManager->Map("MOVE_RIGHT", VirtualKeyCode::KEY_D);
    g_InputManager->Map("MOVE_FRONT", VirtualKeyCode::KEY_W);
    g_InputManager->Map("MOVE_BACK", VirtualKeyCode::KEY_S);
    g_InputManager->Map("MOVE_UP", VirtualKeyCode::KEY_Z);
    g_InputManager->Map("MOVE_DOWN", VirtualKeyCode::KEY_X);
    g_InputManager->Map("ZOOM", MouseEvent::SCROLL);

    g_InputManager->Map("DEBUG", VirtualKeyCode::KEY_SPACE);
    g_InputManager->Map("RESET_SCENE", VirtualKeyCode::KEY_R);

    m_Clock.Start();
    return 0;
}

void MyGame::Finalize() {
    m_Logger->info("MyGame Finalize");
    m_Logger = nullptr;
}

void MyGame::Tick() {
    m_Clock.Tick();

    if (g_InputManager->GetBoolNew("DEBUG")) {
        g_DebugManager->ToggleDebugInfo();
    }

    if (auto camera = g_SceneManager->GetCameraNode().lock()) {
        float move = g_InputManager->GetFloatDelta("ZOOM");
        camera->ApplyTransform(translate(mat4f(1.0f), move * camera->GetCameraLookAt()));
    }

    if (auto light = g_SceneManager->GetSceneLightNode("Light.001").lock()) {
        vec3f translation(0);
        if (g_InputManager->GetBool("MOVE_LEFT")) translation.x -= 0.1f;
        if (g_InputManager->GetBool("MOVE_RIGHT")) translation.x += 0.1f;
        if (g_InputManager->GetBool("MOVE_FRONT")) translation.y -= 0.1f;
        if (g_InputManager->GetBool("MOVE_BACK")) translation.y += 0.1f;
        if (g_InputManager->GetBool("MOVE_UP")) translation.z += 0.1f;
        if (g_InputManager->GetBool("MOVE_DOWN")) translation.z -= 0.1f;

        light->ApplyTransform(translate(mat4f(1.0f), translation));
    }

    g_DebugManager->DrawAxis(scale(mat4f(1.0f), 10.0f));
    g_DebugManager->DrawBox(
        Box{vec3f(-1.0f, -1.0f, -1.0f), vec3f(1.0f, 1.0f, 1.0f)},
        translate(mat4f(1.0f), vec3f(3.0f, 0.0f, 0.0f)),
        vec4f(0.8f, 0.2f, 0.4f, 1.0f));

    if (g_InputManager->GetBoolNew("RESET_SCENE")) {
        g_SceneManager->ResetScene();
    }
}
