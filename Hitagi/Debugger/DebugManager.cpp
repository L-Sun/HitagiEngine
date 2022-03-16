#include "DebugManager.hpp"
#include "GeometryFactory.hpp"
#include "GuiManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/chrono.h>

#include <iostream>
#include <algorithm>

namespace Hitagi {
std::unique_ptr<Debugger::DebugManager> g_DebugManager = std::make_unique<Debugger::DebugManager>();
}
namespace Hitagi::Debugger {
constexpr auto cmp = [](const DebugPrimitive& lhs, const DebugPrimitive& rhs) -> bool {
    return lhs.expires_at > rhs.expires_at;
};

int DebugManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("DebugManager");
    m_Logger->info("Initialize...");
    return 0;
}

void DebugManager::Finalize() {
    m_DebugLine = nullptr;
    m_DebugBox  = nullptr;
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void DebugManager::Tick() {
    while (!m_DebugPrimitives.empty() && m_DebugPrimitives.back().expires_at < std::chrono::high_resolution_clock::now()) {
        std::pop_heap(m_DebugPrimitives.begin(), m_DebugPrimitives.end(), cmp);
        m_DebugPrimitives.pop_back();
    }
    ShowProfilerInfo();
    m_TimingInfo.clear();
}

void DebugManager::ShowProfilerInfo() {
    g_GuiManager->DrawGui([timing_info = m_TimingInfo]() -> void {
        if (ImGui::Begin("Profiler")) {
            ImColor red(0.83f, 0.27f, 0.33f, 1.0f);
            ImColor green(0.53f, 1.0f, 0.29f, 1.0f);

            if (ImGui::BeginTable("Timing table", 2)) {
                for (auto&& [name, timing] : timing_info) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s: ", name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextColored(
                        timing.count() * 1000.0 > 1.0f ? red : green,
                        "%s",
                        fmt::format("{:>10.2%Q %q}", std::chrono::duration<double, std::milli>(timing)).c_str());
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    });
}

void DebugManager::ToggleDebugInfo() {
    m_DrawDebugInfo = !m_DrawDebugInfo;
}

void DebugManager::DrawLine(const vec3f& from, const vec3f& to, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    if (m_DebugLine == nullptr)
        m_DebugLine = Asset::GeometryFactory::Line(vec3f(0.0f, 0.0f, 0.0f), vec3f(1.0f, 0.0f, 0.0f));

    const vec3f dest_direction = normalize(to - from);
    const float length         = (to - from).norm();
    const vec3f rotate_axis    = cross(vec3f(1.0f, 0.0f, 0.0f), dest_direction);
    const float theta          = std::acos(dot(vec3f(1.0f, 0.0f, 0.0f), dest_direction));

    mat4f transform = translate(rotate(scale(mat4f(1.0f), length), theta, rotate_axis), from);
    AddPrimitive(m_DebugLine, transform, color, duration, depth_enabled);
}

void DebugManager::DrawAxis(const mat4f& transform, bool depth_enabled) {
    const vec3f origin{0.0f, 0.0f, 0.0f};
    const vec4f x{1.0f, 0.0f, 0.0f, 1.0f};
    const vec4f y{0.0f, 1.0f, 0.0f, 1.0f};
    const vec4f z{0.0f, 0.0f, 1.0f, 1.0f};
    DrawLine(origin, (transform * x).xyz, vec4f(1, 0, 0, 1), std::chrono::seconds(0), depth_enabled);
    DrawLine(origin, (transform * y).xyz, vec4f(0, 1, 0, 1), std::chrono::seconds(0), depth_enabled);
    DrawLine(origin, (transform * z).xyz, vec4f(0, 0, 1, 1), std::chrono::seconds(0), depth_enabled);
}

void DebugManager::DrawBox(const mat4f& transform, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    if (m_DebugBox == nullptr)
        m_DebugBox = Asset::GeometryFactory::Box(vec3f(-0.5f, -0.5f, -0.5f), vec3f(0.5f, 0.5f, 0.5f));
    AddPrimitive(m_DebugBox, transform, color, duration, depth_enabled);
}

void DebugManager::AddPrimitive(std::shared_ptr<Asset::Geometry> geometry, const mat4f& transform, const vec4f& color, std::chrono::seconds duration, bool depth_enabled) {
    auto node = std::make_shared<Asset::GeometryNode>();
    node->SetSceneObjectRef(geometry);
    node->ApplyTransform(transform);
    m_DebugPrimitives.emplace_back(DebugPrimitive{node, color, std::chrono::high_resolution_clock::now() + duration});
    // make min heap
    std::push_heap(m_DebugPrimitives.begin(), m_DebugPrimitives.end(), cmp);
}

}  // namespace Hitagi::Debugger