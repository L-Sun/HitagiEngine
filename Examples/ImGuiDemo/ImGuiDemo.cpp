#include "ImGuiDemo.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace hitagi;

int ImGuiDemo::Initialize() {
    m_Logger = spdlog::stdout_color_mt("ImGuiDemo");
    return 0;
}

void ImGuiDemo::Finalize() {
    m_Logger->info("ImGuiDemo Finalize");
    m_Logger = nullptr;
}

void ImGuiDemo::Tick() {
    bool open = true;
    g_GuiManager->DrawGui([&]() -> void {
        ImGui::ShowDemoWindow(&open);
    });
}
