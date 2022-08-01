#include "my_game.hpp"

#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/core/thread_manager.hpp>
#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/resource/scene_manager.hpp>
#include <hitagi/gui/gui_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <queue>

using namespace hitagi;
using namespace hitagi::math;

bool MyGame::Initialize() {
    m_Logger = spdlog::stdout_color_mt("MyGame");
    m_Logger->info("Initialize...");

    return true;
}

void MyGame::Finalize() {
    m_Logger->info("MyGame Finalize");
    m_Logger = nullptr;
}

void MyGame::Tick() {
    debug_manager->DrawAxis(mat4f::identity());
    gui_manager->DrawGui([]() {
        bool open = true;
        ImGui::ShowDemoWindow(&open);
    });
    m_Clock.Tick();
}
