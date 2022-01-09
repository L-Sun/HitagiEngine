#include "MyGame.hpp"

#include "FileIOManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace Hitagi;

int MyGame::Initialize() {
    int ret  = 0;
    m_Logger = spdlog::stdout_color_mt("MyGame");
    m_Logger->info("Initialize...");

    if ((ret = m_Editor.Initialize()) != 0) return ret;

    return ret;
}

void MyGame::Finalize() {
    m_Logger->info("MyGame Finalize");
    m_Logger = nullptr;
}

void MyGame::Tick() {
    m_Editor.Tick();
    g_DebugManager->DrawAxis(mat4f(1.0f));
    static float phi = 0;
    phi += std::numbers::inv_pi / 180.0f;
    g_DebugManager->DrawBox(translate(mat4f(1.0f), vec3f(2 * std::sin(phi), 2 * std::cos(phi), 0.0f)), vec4f(1.0f, 0.3f, 0.1f, 1.0f));
    g_GuiManager->DrawGui([&]() { m_Editor.Draw(); });
}
