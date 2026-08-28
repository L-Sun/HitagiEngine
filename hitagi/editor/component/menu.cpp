module;

#include <imgui.h>
#include <imgui_internal.h>
#include <tracy/Tracy.hpp>

module editor;

using namespace hitagi::math;
using namespace hitagi::asset;
using namespace std::literals;

namespace hitagi {
void Editor::MenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) NewScene();
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) RequestOpenScene();
            if (ImGui::MenuItem("Save", "Ctrl+S", false, false)) SaveCurrentScene();
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, false)) RequestSaveSceneAs();
            ImGui::Separator();
            if (ImGui::MenuItem("Import Scene...")) RequestImportScene();
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) m_App.Quit();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            const auto undo_label = std::format("Undo {}", m_CommandStack.GetUndoLabel());
            if (ImGui::MenuItem(undo_label.c_str(), "Ctrl+Z", false, m_CommandStack.CanUndo())) Undo();
            const auto redo_label = std::format("Redo {}", m_CommandStack.GetRedoLabel());
            if (ImGui::MenuItem(redo_label.c_str(), "Ctrl+Y", false, m_CommandStack.CanRedo())) Redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, m_State.GetSelectedEntity() && m_State.GetCurrentScene() && m_State.GetSelectedEntity() != m_State.GetCurrentScene()->GetRootEntity())) DuplicateSelectedEntity();
            if (ImGui::MenuItem("Delete", "Del", false, m_State.GetSelectedEntity() && m_State.GetCurrentScene() && m_State.GetSelectedEntity() != m_State.GetCurrentScene()->GetRootEntity())) DeleteSelectedEntity();
            if (ImGui::MenuItem("Focus Selected", "F", false, static_cast<bool>(m_State.GetSelectedEntity()))) {
                if (m_SceneViewPort) m_SceneViewPort->FocusSelectedEntity();
                Notify("Focused selected entity.");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Scene Graph", nullptr, m_State.IsPanelVisible(EditorPanel::SceneGraph))) {
                m_State.SetPanelVisible(EditorPanel::SceneGraph, !m_State.IsPanelVisible(EditorPanel::SceneGraph));
            }
            if (ImGui::MenuItem("Scene Node Modifier", nullptr, m_State.IsPanelVisible(EditorPanel::SceneNodeModifier))) {
                m_State.SetPanelVisible(EditorPanel::SceneNodeModifier, !m_State.IsPanelVisible(EditorPanel::SceneNodeModifier));
            }
            if (ImGui::MenuItem("Scene Viewer", nullptr, m_State.IsPanelVisible(EditorPanel::SceneViewer))) {
                m_State.SetPanelVisible(EditorPanel::SceneViewer, !m_State.IsPanelVisible(EditorPanel::SceneViewer));
            }
            if (ImGui::MenuItem("Preview", nullptr, m_State.IsPanelVisible(EditorPanel::AssetPreview))) {
                m_State.SetPanelVisible(EditorPanel::AssetPreview, !m_State.IsPanelVisible(EditorPanel::AssetPreview));
            }
            if (ImGui::MenuItem("Asset Explorer", nullptr, m_State.IsPanelVisible(EditorPanel::AssetExplorer))) {
                m_State.SetPanelVisible(EditorPanel::AssetExplorer, !m_State.IsPanelVisible(EditorPanel::AssetExplorer));
            }
            if (ImGui::MenuItem("Debug & Profiling", nullptr, m_State.IsPanelVisible(EditorPanel::DebugProfiling))) {
                m_State.SetPanelVisible(EditorPanel::DebugProfiling, !m_State.IsPanelVisible(EditorPanel::DebugProfiling));
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Select", "Q", m_State.GetActiveTool() == EditorTool::Select)) m_State.SetActiveTool(EditorTool::Select);
            if (ImGui::MenuItem("Translate", "W", m_State.GetActiveTool() == EditorTool::Translate)) m_State.SetActiveTool(EditorTool::Translate);
            if (ImGui::MenuItem("Rotate", "E", m_State.GetActiveTool() == EditorTool::Rotate)) m_State.SetActiveTool(EditorTool::Rotate);
            if (ImGui::MenuItem("Scale", "R", m_State.GetActiveTool() == EditorTool::Scale)) m_State.SetActiveTool(EditorTool::Scale);
            ImGui::Separator();
            if (ImGui::MenuItem("Local Space", "L", m_State.GetCoordinateSpace() == EditorCoordinateSpace::Local)) m_State.SetCoordinateSpace(EditorCoordinateSpace::Local);
            if (ImGui::MenuItem("World Space", "G", m_State.GetCoordinateSpace() == EditorCoordinateSpace::World)) m_State.SetCoordinateSpace(EditorCoordinateSpace::World);
            ImGui::Separator();
            if (ImGui::MenuItem("Play", "F5", m_State.GetMode() != EditorMode::Edit, m_State.GetCurrentScene() != nullptr)) {
                if (m_State.GetMode() == EditorMode::Edit)
                    EnterPlayMode();
                else
                    ExitPlayMode();
            }
            if (ImGui::MenuItem("Pause", "F6", m_State.GetMode() == EditorMode::Pause, m_State.GetMode() != EditorMode::Edit)) TogglePauseMode();
            if (ImGui::MenuItem("Step", "F10", false, m_State.GetMode() != EditorMode::Edit)) StepPlayMode();
            ImGui::Separator();
            if (ImGui::BeginMenu("Scene", m_State.GetCurrentScene() != nullptr)) {
                auto scene = m_State.GetCurrentScene();
                if (ImGui::MenuItem("Create Empty Entity")) {
                    ExecuteCommand(std::make_unique<CreateEmptyEntityCommand>(*scene, scene->GetRootEntity(), "Empty Entity", &m_State));
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::MenuItem("Debug & Profiling", nullptr, m_State.IsPanelVisible(EditorPanel::DebugProfiling))) {
                m_State.SetPanelVisible(EditorPanel::DebugProfiling, !m_State.IsPanelVisible(EditorPanel::DebugProfiling));
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            ImGui::TextUnformatted("Hitagi Editor");
            ImGui::TextUnformatted("Shortcuts: Ctrl+N/O/S, Ctrl+Z/Y, Q/W/E/R, L/G, Del");
            ImGui::EndMenu();
        }

        // System info
        {
            static std::uint64_t smooth_count = 0;
            static float         frame_time   = 0;
            if (smooth_count < m_Clock.TotalTime().count()) {
                smooth_count++;
                frame_time = m_Engine.RenderRuntime().GetFrameTime().count();
            }

            const auto fps = frame_time > 0.0f ? static_cast<unsigned>(1.0f / frame_time) : 0u;
            auto       info = std::format("Memory: {:>4} MiB | {:>4} FPS", m_App.GetMemoryUsage() >> 20, fps);

            ImVec2 info_size = ImGui::CalcTextSize(info.c_str());

            ImGuiStyle& style = ImGui::GetStyle();
            info_size.x += 2 * style.FramePadding.x + style.ItemSpacing.x;

            ImGui::SetCursorPos(ImVec2(ImGui::GetIO().DisplaySize.x - info_size.x, 0));
            ImGui::Text("%s", info.c_str());
        }
        ImGui::EndMainMenuBar();
    }
}

void Editor::Toolbar() {
    ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetFrameHeight()), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 92.0f), ImGuiCond_Always);
    constexpr auto flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("Editor Toolbar", nullptr, flags)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 5.0f));
        ImGui::TextDisabled("FILE");
        ImGui::SameLine();
        if (ImGui::Button("New", ImVec2(54.0f, 0.0f))) NewScene();
        ImGui::SameLine();
        if (ImGui::Button("Open", ImVec2(58.0f, 0.0f))) RequestOpenScene();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_State.GetCurrentScene());
        ImGui::BeginDisabled(true);
        if (ImGui::Button("Save", ImVec2(58.0f, 0.0f))) SaveCurrentScene();
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextDisabled("EDIT");
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_CommandStack.CanUndo());
        if (ImGui::Button("Undo", ImVec2(58.0f, 0.0f))) Undo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_CommandStack.CanRedo());
        if (ImGui::Button("Redo", ImVec2(58.0f, 0.0f))) Redo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextDisabled("RUN");
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_State.GetCurrentScene());
        if (ImGui::Button(m_State.GetMode() == EditorMode::Edit ? "Play" : "Stop", ImVec2(62.0f, 0.0f))) {
            if (m_State.GetMode() == EditorMode::Edit)
                EnterPlayMode();
            else
                ExitPlayMode();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_State.GetMode() == EditorMode::Edit);
        if (ImGui::Button(m_State.GetMode() == EditorMode::Pause ? "Resume" : "Pause", ImVec2(74.0f, 0.0f))) TogglePauseMode();
        ImGui::SameLine();
        if (ImGui::Button("Step", ImVec2(58.0f, 0.0f))) StepPlayMode();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        const auto scene_name = m_State.GetCurrentScene() ? std::string{m_State.GetCurrentScene()->GetName()} : std::string{"No scene"};
        ImGui::Text("Mode %s  Scene %s%s", EditorModeName(m_State.GetMode()).data(), scene_name.c_str(), m_State.IsDirty() ? "*" : "");

        ImGui::Separator();

        ImGui::TextDisabled("TOOL");
        ImGui::SameLine();
        if (ImGui::RadioButton("Select##ToolSelect", m_State.GetActiveTool() == EditorTool::Select)) m_State.SetActiveTool(EditorTool::Select);
        ImGui::SameLine();
        if (ImGui::RadioButton("Move##ToolMove", m_State.GetActiveTool() == EditorTool::Translate)) m_State.SetActiveTool(EditorTool::Translate);
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate##ToolRotate", m_State.GetActiveTool() == EditorTool::Rotate)) m_State.SetActiveTool(EditorTool::Rotate);
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale##ToolScale", m_State.GetActiveTool() == EditorTool::Scale)) m_State.SetActiveTool(EditorTool::Scale);
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextDisabled("SPACE");
        ImGui::SameLine();
        if (ImGui::RadioButton("Local##SpaceLocal", m_State.GetCoordinateSpace() == EditorCoordinateSpace::Local)) m_State.SetCoordinateSpace(EditorCoordinateSpace::Local);
        ImGui::SameLine();
        if (ImGui::RadioButton("World##SpaceWorld", m_State.GetCoordinateSpace() == EditorCoordinateSpace::World)) m_State.SetCoordinateSpace(EditorCoordinateSpace::World);
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextDisabled("SNAP");
        ImGui::SameLine();
        bool snap_enabled = m_State.IsSnapEnabled();
        if (ImGui::Checkbox("Enable##SnapEnable", &snap_enabled)) m_State.SetSnapEnabled(snap_enabled);
        ImGui::SameLine();
        float translate_snap = m_State.GetTranslateSnap();
        ImGui::SetNextItemWidth(92.0f);
        if (ImGui::DragFloat("Move##SnapMove", &translate_snap, 0.05f, 0.01f, 100.0f, "%.2f")) m_State.SetTranslateSnap(translate_snap);
        ImGui::SameLine();
        float rotate_snap_degrees = math::rad2deg(m_State.GetRotateSnap());
        ImGui::SetNextItemWidth(92.0f);
        if (ImGui::DragFloat("Rot##SnapRotate", &rotate_snap_degrees, 1.0f, 1.0f, 180.0f, "%.0f")) m_State.SetRotateSnap(math::deg2rad(rotate_snap_degrees));
        ImGui::SameLine();
        float scale_snap = m_State.GetScaleSnap();
        ImGui::SetNextItemWidth(92.0f);
        if (ImGui::DragFloat("Scale##SnapScale", &scale_snap, 0.01f, 0.001f, 10.0f, "%.2f")) m_State.SetScaleSnap(scale_snap);
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_State.GetSelectedEntity());
        if (ImGui::Button("Focus", ImVec2(70.0f, 0.0f)) && m_SceneViewPort) m_SceneViewPort->FocusSelectedEntity();
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
    }
    ImGui::End();
}

void Editor::FileImporter() {
    m_FileDialog.Display();

    if (m_FileDialog.HasSelected()) {
        const auto selected = m_FileDialog.GetSelected();
        if (m_FileDialogMode == EditorFileDialogMode::OpenScene || m_FileDialogMode == EditorFileDialogMode::ImportScene) {
            OpenScene(selected);
        }
        m_FileDialogMode = EditorFileDialogMode::None;
        m_FileDialog.ClearSelected();
    }

}
}  // namespace hitagi
