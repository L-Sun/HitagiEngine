#include "imgui_demo.hpp"

#include <hitagi/gui/gui_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace hitagi;

bool ImGuiDemo::Initialize() {
    m_Logger = spdlog::stdout_color_mt("ImGuiDemo");
    return true;
}

void ImGuiDemo::Finalize() {
    m_Logger->info("ImGuiDemo Finalize");
    m_Logger = nullptr;
}

void ImGuiDemo::Tick() {
    bool open = true;
    gui_manager->DrawGui([&]() -> void {
        ImGui::ShowDemoWindow(&open);
    });
}
