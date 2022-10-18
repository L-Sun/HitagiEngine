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
                std::function<void(const RuntimeModule*)> module_inspector = [&](const RuntimeModule* module) {
                    if (ImGui::TreeNode(module->GetName().data())) {
                        {
                            auto   time_cost = std::chrono::duration_cast<std::chrono::milliseconds>(module->GetProfileTime());
                            ImVec4 color;
                            if (time_cost >= 10ms) {
                                color = ImVec4(1, 0, 0, 1);
                            } else if (1ms <= time_cost && time_cost < 10ms) {
                                color = ImVec4(0.95f, 0.77f, 0.06f, 1.0f);
                            } else {
                                color = ImVec4(0.53f, 1.0f, 0.29f, 1.0f);
                            }
                            ImGui::Text("Time cost: ");
                            ImGui::SameLine();
                            ImGui::TextColored(color, "%s", fmt::format("{}", time_cost).c_str());
                        }
                        for (const auto& sub_module : module->GetSubModules()) {
                            module_inspector(sub_module);
                        }
                        ImGui::TreePop();
                    }
                };
                module_inspector(engine);
            }
        }
        ImGui::End();
    });

    RuntimeModule::Tick();
}

}  // namespace hitagi