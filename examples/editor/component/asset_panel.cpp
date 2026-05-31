module;

#include <imgui.h>
#include <imgui_internal.h>
#include <tracy/Tracy.hpp>

module editor;
import magic_enum;

using namespace hitagi::math;
using namespace hitagi::asset;
using namespace std::literals;

namespace hitagi {
namespace {
constexpr auto kWorkbenchWindowFlags = ImGuiWindowFlags_NoCollapse;
}  // namespace

auto ContainsCaseInsensitive(std::string_view text, std::string_view filter) -> bool {
    if (filter.empty()) return true;
    if (filter.size() > text.size()) return false;
    for (std::size_t i = 0; i + filter.size() <= text.size(); ++i) {
        bool matched = true;
        for (std::size_t j = 0; j < filter.size(); ++j) {
            if (asset::detail::ascii_lower(text[i + j]) != asset::detail::ascii_lower(filter[j])) {
                matched = false;
                break;
            }
        }
        if (matched) return true;
    }
    return false;
}
void Editor::RefreshAssetBrowser() {
    EditorAssetBrowserController controller{m_AssetBrowserModel};
    controller.SetRoot(m_App.GetConfig().asset_root_path);
    controller.RefreshIfNeeded([](const std::filesystem::path& path) {
        return ClassifyEditorAssetPath(path);
    });
}

void Editor::ImportAssetFromBrowser(const EditorAssetBrowserEntry& entry) {
    try {
        auto* asset_manager = asset::AssetManager::Get();
        m_State.SetSelectedAsset(entry.path, entry.kind);
        m_LastImportedAssetPath.clear();
        m_LastImportedAssetUUID.clear();

        if (entry.kind == EditorAssetKind::Scene || entry.kind == EditorAssetKind::Model) {
            auto scene = asset_manager->ImportScene(entry.path);
            if (scene) {
                m_LastImportedAssetPath = entry.path;
                m_LastImportedAssetUUID = std::format("{}", scene->GetUUID());
                SetCurrentScene(std::move(scene));
                m_CurrentScenePath   = entry.path;
                m_AssetBrowserStatus = std::format("Imported scene: {}", entry.relative_path);
                Notify(m_AssetBrowserStatus);
            }
        } else if (entry.kind == EditorAssetKind::Texture) {
            auto texture = asset_manager->ImportTexture(entry.path);
            if (texture) {
                texture->InitGPUData(m_Engine.RenderRuntime().GetRenderGraph().GetDevice());
                m_SelectedTexturePreview = texture;
                m_ImageViewer->SetTexture(texture);
                m_LastImportedAssetPath = entry.path;
                m_LastImportedAssetUUID = std::format("{}", texture->GetUUID());
                m_AssetBrowserStatus    = std::format("Previewing texture: {}", entry.relative_path);
                Notify(m_AssetBrowserStatus);
            }
        } else if (entry.kind == EditorAssetKind::Material) {
            auto material = asset_manager->ImportMaterial(entry.path);
            if (material) {
                m_LastImportedAssetPath = entry.path;
                m_LastImportedAssetUUID = std::format("{}", material->GetUUID());
                m_AssetBrowserStatus    = std::format("Imported material: {}", entry.relative_path);
                Notify(m_AssetBrowserStatus);
            }
        }
    } catch (const std::exception& error) {
        m_AssetBrowserStatus = std::format("Import failed for {}: {}", entry.relative_path, error.what());
        Notify(m_AssetBrowserStatus);
    }
}

void Editor::AssetExplorer() {
    if (!m_State.IsPanelVisible(EditorPanel::AssetExplorer)) return;

    auto open = true;
    if (ImGui::Begin("Assets", &open, kWorkbenchWindowFlags)) {
        RefreshAssetBrowser();

        const auto& asset_root = m_AssetBrowserModel.GetRoot();
        ImGui::Text("Root");
        ImGui::SameLine();
        ImGui::TextUnformatted(asset_root.string().c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh")) {
            m_AssetBrowserModel.RequestRefresh();
            RefreshAssetBrowser();
        }

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##AssetSearch", "Search assets", m_AssetSearchBuffer.data(), m_AssetSearchBuffer.size());

        if (!m_AssetBrowserStatus.empty()) {
            ImGui::TextWrapped("%s", m_AssetBrowserStatus.c_str());
        }

        const auto selected_path = m_State.GetSelectedAssetPath();
        if (!selected_path.empty()) {
            ImGui::SeparatorText("Selected Asset");
            ImGui::Text("Name: %s", selected_path.filename().string().c_str());
            ImGui::Text("Type: %s", EditorAssetKindName(m_State.GetSelectedAssetKind()).data());
            ImGui::TextWrapped("Path: %s", selected_path.string().c_str());
            if (selected_path == m_LastImportedAssetPath && !m_LastImportedAssetUUID.empty()) {
                ImGui::Text("UUID: %s", m_LastImportedAssetUUID.c_str());
            }
        }

        if (!m_AssetBrowserModel.GetLastError().empty()) {
            ImGui::TextWrapped("%s", m_AssetBrowserModel.GetLastError().data());
            ImGui::End();
            return;
        }

        constexpr ImGuiTableFlags table_flags =
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_BordersInnerV;

        if (ImGui::BeginTable("AssetBrowserTable", 3, table_flags, ImVec2(0, 0))) {
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 92.0f);
            ImGui::TableHeadersRow();

            const auto filter = std::string_view{m_AssetSearchBuffer.data()};
            if (m_AssetFilterRevision != m_AssetBrowserModel.GetRevision() || m_AssetFilterCache != filter) {
                ZoneScopedN("AssetBrowser Filter Cache");
                m_AssetFilterCache    = filter;
                m_AssetFilterRevision = m_AssetBrowserModel.GetRevision();
                m_FilteredAssetIndices.clear();

                const auto& entries = m_AssetBrowserModel.GetEntries();
                m_FilteredAssetIndices.reserve(entries.size());
                for (std::size_t index = 0; index < entries.size(); ++index) {
                    if (ContainsCaseInsensitive(entries[index].relative_path, filter)) {
                        m_FilteredAssetIndices.emplace_back(index);
                    }
                }
            }

            const auto&      entries = m_AssetBrowserModel.GetEntries();
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(m_FilteredAssetIndices.size()));
            while (clipper.Step()) {
                for (int visible_index = clipper.DisplayStart; visible_index < clipper.DisplayEnd; ++visible_index) {
                    const auto& entry = entries[m_FilteredAssetIndices[static_cast<std::size_t>(visible_index)]];

                    ImGui::TableNextRow();
                    ImGui::PushID(entry.relative_path.c_str());

                    ImGui::TableSetColumnIndex(0);
                    const auto kind_name = entry.kind == EditorAssetKind::Unknown ? std::string_view{"-"} : magic_enum::enum_name(entry.kind);
                    ImGui::TextUnformatted(kind_name.data());

                    ImGui::TableSetColumnIndex(1);
                    const auto selected = entry.path == m_State.GetSelectedAssetPath();
                    if (ImGui::Selectable(entry.display_name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                        m_State.SetSelectedAsset(entry.path, entry.kind);
                        if (entry.kind == EditorAssetKind::Unknown) {
                            m_AssetBrowserStatus = std::format("Unsupported asset type: {}", entry.relative_path);
                        } else {
                            m_AssetBrowserStatus.clear();
                        }
                    }
                    if (ImGui::BeginDragDropSource()) {
                        const auto payload = entry.path.string();
                        ImGui::SetDragDropPayload("HITAGI_ASSET_PATH", payload.c_str(), payload.size() + 1);
                        ImGui::Text("%s", entry.relative_path.c_str());
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", entry.relative_path.c_str());
                    }

                    ImGui::TableSetColumnIndex(2);
                    const auto can_import = entry.kind != EditorAssetKind::Unknown;
                    if (!can_import) ImGui::BeginDisabled(true);
                    const auto action_label = entry.kind == EditorAssetKind::Texture ? "Preview" : "Import";
                    if (ImGui::SmallButton(action_label)) {
                        ImportAssetFromBrowser(entry);
                    }
                    if (!can_import) ImGui::EndDisabled();

                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
    if (!open) m_State.SetPanelVisible(EditorPanel::AssetExplorer, false);
}
}  // namespace hitagi
