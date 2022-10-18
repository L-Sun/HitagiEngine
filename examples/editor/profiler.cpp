#include "profiler.hpp"
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/application.hpp>
#include <hitagi/engine.hpp>
#include "hitagi/core/runtime_module.hpp"

#include <imgui.h>
#include <fmt/chrono.h>

using namespace std::literals;

namespace hitagi {
bool Profiler::Initialize() {
    RuntimeModule::Initialize();
    m_Clock.Start();
    return true;
}

void Profiler::Tick() {
    engine->SetProfileTime(m_Clock.DeltaTime());
    m_Clock.Tick();

    gui_manager->DrawGui([this]() {
        if (ImGui::Begin(GetName().data(), &m_Open)) {
            // Memory usage
            {
                ImGui::Text("%s", fmt::format("Memory Usage: {}Mb", app->GetMemoryUsage() >> 20).c_str());
            }
            ImGui::Separator();
            // Module time cost
            {
                static ImGuiTableFlags flags           = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
                static float           TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;

                if (ImGui::BeginTable("Module inspector", 2, flags)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                    ImGui::TableSetupColumn("Time cost", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 4.0f);
                    ImGui::TableHeadersRow();

                    auto time_cost_text = [](const RuntimeModule* module) {
                        auto   time_cost = std::chrono::duration_cast<std::chrono::milliseconds>(module->GetProfileTime());
                        ImVec4 color;
                        if (time_cost >= 10ms) {
                            color = ImVec4(1, 0, 0, 1);
                        } else if (1ms <= time_cost && time_cost < 10ms) {
                            color = ImVec4(0.95f, 0.77f, 0.06f, 1.0f);
                        } else {
                            color = ImVec4(0.53f, 1.0f, 0.29f, 1.0f);
                        }
                        ImGui::TextColored(color, "%s", fmt::format("{}", time_cost).c_str());
                    };

                    std::function<void(const RuntimeModule*)> module_inspector = [&](const RuntimeModule* module) {
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(1);
                        time_cost_text(module);

                        auto sub_modules = module->GetSubModules();
                        ImGui::TableSetColumnIndex(0);
                        if (!sub_modules.empty()) {
                            if (ImGui::TreeNodeEx(module->GetName().data(), ImGuiTreeNodeFlags_SpanFullWidth)) {
                                for (const auto& sub_module : module->GetSubModules()) {
                                    module_inspector(sub_module);
                                }
                                ImGui::TreePop();
                            }
                        } else {
                            ImGui::TreeNodeEx(module->GetName().data(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
                        }
                    };

                    module_inspector(engine);
                    ImGui::EndTable();
                }
            }
        }
        ImGui::End();
    });

    RuntimeModule::Tick();
}

}  // namespace hitagi