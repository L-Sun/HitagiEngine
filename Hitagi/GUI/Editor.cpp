#include "Editor.hpp"
#include "FileIOManager.hpp"

#include <imgui.h>

namespace Hitagi::Gui {
void Editor::MainMenu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                FileExplorer();
            }
        }

        ImGui::EndMainMenuBar();
    }
}

void Editor::FileExplorer() {
    if (ImGui::BeginPopup("File browser")) {
                ImGui::EndPopup();
    }
}

}  // namespace Hitagi::Gui