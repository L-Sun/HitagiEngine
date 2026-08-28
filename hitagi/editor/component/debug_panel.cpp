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

auto BuildEditorRenderGraphDebugSnapshot(std::string_view dot) -> EditorRenderGraphDebugSnapshot {
    EditorRenderGraphDebugSnapshot snapshot;

    std::size_t cursor = 0;
    while (cursor < dot.size()) {
        const auto line_end = dot.find('\n', cursor);
        const auto line     = dot.substr(cursor, line_end == std::string_view::npos ? dot.size() - cursor : line_end - cursor);
        cursor              = line_end == std::string_view::npos ? dot.size() : line_end + 1;

        const auto bracket = line.find('[');
        if (bracket == std::string_view::npos || line.find("->") != std::string_view::npos) continue;

        const auto id_text = line.substr(0, bracket);
        std::uint64_t handle = 0;
        auto first_digit = id_text.find_first_of("0123456789");
        if (first_digit == std::string_view::npos) continue;
        for (auto ch : id_text.substr(first_digit)) {
            if (ch < '0' || ch > '9') break;
            handle = handle * 10 + static_cast<std::uint64_t>(ch - '0');
        }

        const auto label = line.find("label=\"", bracket);
        if (label == std::string_view::npos) continue;
        const auto name_begin = label + std::string_view("label=\"").size();
        const auto name_end   = line.find("\\nhandle:", name_begin);
        if (name_end == std::string_view::npos) continue;

        EditorRenderGraphDebugNode node{
            .handle   = handle,
            .name     = std::pmr::string(line.substr(name_begin, name_end - name_begin)),
            .resource = line.find("shape=box", bracket) != std::string_view::npos,
        };
        if (node.resource) {
            snapshot.resources.emplace_back(std::move(node));
        } else {
            snapshot.passes.emplace_back(std::move(node));
        }
    }

    return snapshot;
}

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
                const auto& render_graph    = m_Engine.RenderRuntime().GetRenderGraph();
                const auto  transient_stats = render_graph.GetTransientPoolStats();
                ImGui::Text("Frame: %llu", static_cast<unsigned long long>(render_graph.GetFrameIndex()));
                ImGui::Text("Transient textures: %llu", static_cast<unsigned long long>(transient_stats.texture_count));
                ImGui::Text("Transient texture pool: %llu MiB", static_cast<unsigned long long>(transient_stats.texture_bytes >> 20));
                ImGui::Text("Transient buffers: %llu", static_cast<unsigned long long>(transient_stats.buffer_count));
                ImGui::Text("Transient buffer pool: %llu MiB", static_cast<unsigned long long>(transient_stats.buffer_bytes >> 20));
                const auto snapshot = BuildEditorRenderGraphDebugSnapshot(render_graph.ToDot());
                if (ImGui::TreeNode("Passes")) {
                    if (snapshot.passes.empty()) {
                        ImGui::TextUnformatted("-");
                    } else if (ImGui::BeginTable("RenderGraphPasses", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
                        ImGui::TableSetupColumn("Handle");
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableHeadersRow();
                        for (const auto& pass : snapshot.passes) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%llu", static_cast<unsigned long long>(pass.handle));
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(pass.name.empty() ? "<unnamed>" : pass.name.c_str());
                        }
                        ImGui::EndTable();
                    }
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Resources")) {
                    if (snapshot.resources.empty()) {
                        ImGui::TextUnformatted("-");
                    } else if (ImGui::BeginTable("RenderGraphResources", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
                        ImGui::TableSetupColumn("Handle");
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableHeadersRow();
                        for (const auto& resource : snapshot.resources) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%llu", static_cast<unsigned long long>(resource.handle));
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(resource.name.empty() ? "<unnamed>" : resource.name.c_str());
                        }
                        ImGui::EndTable();
                    }
                    ImGui::TreePop();
                }
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
