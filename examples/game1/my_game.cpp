#include "my_game.hpp"

#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/core/thread_manager.hpp>
#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/asset/asset_manager.hpp>

#include <imgui.h>

#include <queue>

using namespace hitagi;
using namespace hitagi::math;

MyGame::MyGame() : RuntimeModule("MyGame") {
    auto scene = asset_manager->ImportScene("./assets/scenes/test.fbx");
}

void MyGame::Tick() {
    debug_manager->DrawAxis(mat4f::identity());
    gui_manager->DrawGui([]() {
        static bool open = true;
        ImGui::ShowDemoWindow(&open);
    });
    m_Clock.Tick();
}
