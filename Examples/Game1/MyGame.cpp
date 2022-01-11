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

    auto& scene = g_SceneManager->GetScene();
    for (auto&& [name, bone] : scene.bone_nodes) {
        if (auto parent = bone->GetParent().lock()) {
            vec3f from = (parent->GetCalculatedTransformation() * vec4f(0, 0, 0, 1)).xyz;
            vec3f to   = (bone->GetCalculatedTransformation() * vec4f(0, 0, 0, 1)).xyz;
            g_DebugManager->DrawLine(from, to, vec4f(0.2f, 0.3f, 0.4f, 1.0f));
        }
    }

    g_GuiManager->DrawGui([&]() { m_Editor.Draw(); });
}
