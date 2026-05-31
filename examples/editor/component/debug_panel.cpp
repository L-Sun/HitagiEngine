module;

#include <imgui.h>
#include <imgui_internal.h>
#include <tracy/Tracy.hpp>

module editor;

using namespace hitagi::math;
using namespace hitagi::asset;
using namespace std::literals;

namespace hitagi {
namespace {
constexpr auto kWorkbenchWindowFlags = ImGuiWindowFlags_NoCollapse;

}  // namespace
void Editor::DebugProfilingPanel() {
    if (!m_State.IsPanelVisible(EditorPanel::DebugProfiling)) return;

    auto open = true;
    if (ImGui::Begin("Console / Stats", &open, kWorkbenchWindowFlags)) {
        const auto frame_time = m_Engine.RenderRuntime().GetFrameTime().count();
        const auto fps        = frame_time > 0.0f ? 1.0f / frame_time : 0.0f;

        if (ImGui::BeginTabBar("DebugProfilingTabs")) {
            if (ImGui::BeginTabItem("Frame")) {
                ImGui::Text("Frame: %llu", static_cast<unsigned long long>(m_FrameIndex));
                ImGui::Text("Render frame time: %.3f ms", frame_time * 1000.0f);
                ImGui::Text("FPS: %.1f", fps);
                ImGui::Text("Memory: %llu MiB", static_cast<unsigned long long>(m_App.GetMemoryUsage() >> 20));
                ImGui::Text("Window: %u x %u", m_App.GetWindowWidth(), m_App.GetWindowHeight());
                ImGui::Text("Viewport render scale: %.2f", 0.75f);
                ImGui::Text("Mode: %s", EditorModeName(m_State.GetMode()).data());
                ImGui::Text("Dirty: %s", m_State.IsDirty() ? "yes" : "no");
                ImGui::Text("Undo: %s", m_CommandStack.GetUndoLabel().empty() ? "-" : m_CommandStack.GetUndoLabel().data());
                ImGui::Text("Redo: %s", m_CommandStack.GetRedoLabel().empty() ? "-" : m_CommandStack.GetRedoLabel().data());
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("RenderGraph")) {
                const auto transient_stats = m_Engine.RenderRuntime().GetRenderGraph().GetTransientPoolStats();
                ImGui::Text("Frame: %llu", static_cast<unsigned long long>(m_Engine.RenderRuntime().GetRenderGraph().GetFrameIndex()));
                ImGui::Text("Transient textures: %llu", static_cast<unsigned long long>(transient_stats.texture_count));
                ImGui::Text("Transient texture pool: %llu MiB", static_cast<unsigned long long>(transient_stats.texture_bytes >> 20));
                ImGui::Text("Transient buffers: %llu", static_cast<unsigned long long>(transient_stats.buffer_count));
                ImGui::Text("Transient buffer pool: %llu MiB", static_cast<unsigned long long>(transient_stats.buffer_bytes >> 20));
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Scene")) {
                if (const auto scene = m_State.GetCurrentScene()) {
                    ImGui::Text("Scene: %s", scene->GetName().data());
                    ImGui::Text("Entities: %llu", static_cast<unsigned long long>(CountSceneEntities(*scene)));
                    ImGui::Text("Mesh entities: %llu", static_cast<unsigned long long>(scene->GetMeshEntities().size()));
                    ImGui::Text("Camera entities: %llu", static_cast<unsigned long long>(scene->GetCameraEntities().size()));
                    ImGui::Text("Light entities: %llu", static_cast<unsigned long long>(scene->GetLightEntities().size()));
                    if (const auto selected = m_State.GetSelectedEntity()) {
                        ImGui::Text("Selected entity: %llu", static_cast<unsigned long long>(selected.GetId()));
                        if (selected.Has<asset::MetaInfo>()) {
                            ImGui::Text("Selected name: %s", selected.Get<asset::MetaInfo>().name.data());
                        }
                    } else {
                        ImGui::TextUnformatted("Selected entity: -");
                    }
                } else {
                    ImGui::TextUnformatted("No scene loaded.");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Assets")) {
                const auto selected_asset = m_State.GetSelectedAssetPath();
                if (selected_asset.empty()) {
                    ImGui::TextUnformatted("No asset selected.");
                } else {
                    ImGui::Text("Selected type: %s", EditorAssetKindName(m_State.GetSelectedAssetKind()).data());
                    ImGui::TextWrapped("Selected path: %s", selected_asset.string().c_str());
                }
                if (!m_AssetBrowserStatus.empty()) {
                    ImGui::SeparatorText("Last asset event");
                    ImGui::TextWrapped("%s", m_AssetBrowserStatus.c_str());
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Console")) {
                if (ImGui::BeginChild("EditorConsole", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
                    for (const auto& message : m_EditorNotifications) {
                        ImGui::TextWrapped("%s", message.c_str());
                    }
                    if (m_EditorNotifications.empty()) {
                        ImGui::TextUnformatted("No editor notifications.");
                    }
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    if (!open) m_State.SetPanelVisible(EditorPanel::DebugProfiling, false);
}
}  // namespace hitagi
