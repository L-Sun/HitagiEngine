module;

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <fstream>
#include <functional>
#include <tracy/Tracy.hpp>

module editor;
import magic_enum;

using namespace hitagi::math;
using namespace hitagi::asset;
using namespace std::literals;

namespace hitagi {
namespace {
constexpr auto kWorkbenchWindowFlags = ImGuiWindowFlags_NoCollapse;
constexpr auto kMaxTextPreviewBytes  = 1024u * 1024u;

auto IsPreviewableAsset(const EditorAssetBrowserEntry& entry) noexcept -> bool {
    return entry.kind == EditorAssetKind::Texture ||
           entry.kind == EditorAssetKind::Material ||
           entry.kind == EditorAssetKind::Shader;
}

auto IsGeneratedMaterialPreviewAsset(const EditorAssetBrowserEntry& entry) -> bool {
    return entry.kind == EditorAssetKind::Material &&
           entry.path.filename().generic_string().ends_with(".material.json");
}

auto IsImportableAsset(const EditorAssetBrowserEntry& entry) -> bool {
    return entry.kind != EditorAssetKind::Unknown &&
           entry.kind != EditorAssetKind::Shader &&
           !IsGeneratedMaterialPreviewAsset(entry);
}

auto AssetPathPart(std::string_view path, std::size_t depth) noexcept -> std::string_view {
    auto start = std::size_t{0};
    for (std::size_t current_depth = 0; current_depth < depth; ++current_depth) {
        start = path.find('/', start);
        if (start == std::string_view::npos) return {};
        ++start;
    }

    const auto end = path.find('/', start);
    return path.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
}

auto AssetPathPartCount(std::string_view path) noexcept -> std::size_t {
    if (path.empty()) return 0;
    return static_cast<std::size_t>(std::ranges::count(path, '/')) + 1;
}

constexpr auto AsciiLower(char value) noexcept -> char {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}
}  // namespace

auto ContainsCaseInsensitive(std::string_view text, std::string_view filter) -> bool {
    if (filter.empty()) return true;
    if (filter.size() > text.size()) return false;
    for (std::size_t i = 0; i + filter.size() <= text.size(); ++i) {
        bool matched = true;
        for (std::size_t j = 0; j < filter.size(); ++j) {
            if (AsciiLower(text[i + j]) != AsciiLower(filter[j])) {
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
            m_CookContext.Clear();
            auto scene = ImportEditorScene(
                entry.path,
                entry.path.parent_path(),
                [asset_manager](std::string_view name) {
                    return asset_manager ? asset_manager->GetMaterial(name) : nullptr;
                },
                nullptr,
                m_LaunchOptions.material_processor,
                std::addressof(m_CookContext));
            if (scene) {
                if (asset_manager) asset_manager->AddScene(scene);
                m_LastImportedAssetPath = entry.path;
                m_LastImportedAssetUUID = std::format("{}", scene->GetUUID());
                SetCurrentScene(std::move(scene));
                m_CurrentScenePath   = entry.path;
                m_AssetBrowserStatus = std::format("Imported scene: {}", entry.relative_path);
                m_AssetBrowserModel.RequestRefresh();
                Notify(m_AssetBrowserStatus);
            }
        } else if (entry.kind == EditorAssetKind::Texture) {
            auto texture = asset_manager->ImportTexture(entry.path);
            if (texture) {
                texture->Load({.device = m_Engine.RenderRuntime().GetRenderGraph().GetDevice()});
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

void Editor::PreviewAssetFromBrowser(const EditorAssetBrowserEntry& entry) {
    m_State.SetSelectedAsset(entry.path, entry.kind);
    m_AssetPreviewPath = entry.path;
    m_AssetPreviewKind = entry.kind;
    m_AssetPreviewText.clear();
    m_AssetPreviewStatus.clear();
    m_State.SetPanelVisible(EditorPanel::AssetPreview, true);

    try {
        if (entry.kind == EditorAssetKind::Texture) {
            auto texture = asset::AssetManager::Get()->ImportTexture(entry.path);
            if (texture) {
                texture->Load({.device = m_Engine.RenderRuntime().GetRenderGraph().GetDevice()});
                m_SelectedTexturePreview = texture;
                m_AssetPreviewStatus     = std::format("Previewing texture: {}", entry.relative_path);
                Notify(m_AssetPreviewStatus);
            }
            return;
        }

        if (entry.kind != EditorAssetKind::Material && entry.kind != EditorAssetKind::Shader) {
            m_AssetPreviewStatus = std::format("Preview is not available for {}", entry.relative_path);
            return;
        }

        std::error_code ec;
        const auto      file_size = std::filesystem::file_size(entry.path, ec);
        if (ec) {
            m_AssetPreviewStatus = std::format("Preview failed for {}: {}", entry.relative_path, ec.message());
            return;
        }

        std::ifstream input(entry.path, std::ios::binary);
        if (!input) {
            m_AssetPreviewStatus = std::format("Preview failed for {}: can not open file", entry.relative_path);
            return;
        }

        const auto  bytes_to_read = static_cast<std::size_t>(std::min<std::uintmax_t>(file_size, kMaxTextPreviewBytes));
        std::string preview(bytes_to_read, '\0');
        input.read(preview.data(), static_cast<std::streamsize>(preview.size()));
        preview.resize(static_cast<std::size_t>(input.gcount()));

        m_AssetPreviewText.assign(preview.begin(), preview.end());
        m_AssetPreviewStatus = file_size > kMaxTextPreviewBytes
                                   ? std::format("Previewing first {} KiB of {}", kMaxTextPreviewBytes / 1024u, entry.relative_path)
                                   : std::format("Previewing {}", entry.relative_path);
    } catch (const std::exception& error) {
        m_AssetPreviewStatus = std::format("Preview failed for {}: {}", entry.relative_path, error.what());
        Notify(m_AssetPreviewStatus);
    }
}

void Editor::DrawAssetPreview() {
    ImGui::TextUnformatted(m_AssetPreviewPath.filename().string().c_str());
    if (!m_AssetPreviewStatus.empty()) {
        ImGui::TextWrapped("%s", m_AssetPreviewStatus.c_str());
    }

    if (m_AssetPreviewKind == EditorAssetKind::Texture) {
        if (const auto texture = m_SelectedTexturePreview) {
            auto&      render_graph = m_Engine.RenderRuntime().GetRenderGraph();
            const auto image_id     = m_Engine.GuiManager().ReadTexture(render_graph.Import(texture->GetGPUData()));
            const auto available    = ImGui::GetContentRegionAvail();
            const auto scale        = std::min(
                available.x / static_cast<float>(texture->Width()),
                available.y / static_cast<float>(texture->Height()));
            const auto image_size = ImVec2(
                std::max(1.0f, static_cast<float>(texture->Width()) * std::min(scale, 1.0f)),
                std::max(1.0f, static_cast<float>(texture->Height()) * std::min(scale, 1.0f)));
            ImGui::Image(image_id, image_size);
        }
        return;
    }

    if (m_AssetPreviewKind == EditorAssetKind::Material || m_AssetPreviewKind == EditorAssetKind::Shader) {
        const auto available = ImGui::GetContentRegionAvail();
        const auto height    = std::max(120.0f, available.y);
        ImGui::InputTextMultiline(
            "##AssetTextPreview",
            const_cast<char*>(m_AssetPreviewText.c_str()),
            m_AssetPreviewText.size() + 1,
            ImVec2(-1.0f, height),
            ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoHorizontalScroll);
    }
}

void Editor::AssetPreview() {
    if (!m_State.IsPanelVisible(EditorPanel::AssetPreview)) return;

    auto open = true;
    if (const auto* viewport_window = ImGui::FindWindowByName("Viewport");
        viewport_window != nullptr && viewport_window->DockId != 0) {
        ImGui::SetNextWindowDockID(viewport_window->DockId, ImGuiCond_Appearing);
    }
    if (ImGui::Begin("Preview", &open, kWorkbenchWindowFlags)) {
        if (m_AssetPreviewPath.empty()) {
            ImGui::TextUnformatted("No asset selected for preview.");
        } else {
            DrawAssetPreview();
        }
    }
    ImGui::End();

    if (!open) m_State.SetPanelVisible(EditorPanel::AssetPreview, false);
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

        if (!m_AssetBrowserModel.GetLastError().empty()) {
            ImGui::TextWrapped("%s", m_AssetBrowserModel.GetLastError().data());
            ImGui::End();
            return;
        }

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

        const auto& entries = m_AssetBrowserModel.GetEntries();
        if (ImGui::BeginChild("AssetTree", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
            std::function<void(std::size_t, std::size_t, std::size_t)> draw_tree;
            draw_tree = [&](std::size_t begin, std::size_t end, std::size_t depth) {
                for (auto i = begin; i < end;) {
                    const auto& entry = entries[m_FilteredAssetIndices[i]];
                    const auto  part  = AssetPathPart(entry.relative_path, depth);
                    if (part.empty()) {
                        ++i;
                        continue;
                    }

                    auto next = i + 1;
                    while (next < end && AssetPathPart(entries[m_FilteredAssetIndices[next]].relative_path, depth) == part) {
                        ++next;
                    }

                    const auto is_file = AssetPathPartCount(entry.relative_path) == depth + 1;
                    if (!is_file) {
                        const auto folder_name = std::string{part};
                        ImGui::PushID(std::format("{}:{}", depth, folder_name).c_str());
                        ImGuiTreeNodeFlags folder_flags = ImGuiTreeNodeFlags_SpanAvailWidth;
                        if (!filter.empty()) folder_flags |= ImGuiTreeNodeFlags_DefaultOpen;
                        const auto open_folder = ImGui::TreeNodeEx(folder_name.c_str(), folder_flags);
                        if (open_folder) {
                            draw_tree(i, next, depth + 1);
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                        i = next;
                        continue;
                    }

                    ImGui::PushID(entry.relative_path.c_str());
                    const auto         selected = entry.path == m_State.GetSelectedAssetPath();
                    ImGuiTreeNodeFlags file_flags =
                        ImGuiTreeNodeFlags_Leaf |
                        ImGuiTreeNodeFlags_NoTreePushOnOpen |
                        ImGuiTreeNodeFlags_SpanAvailWidth;
                    if (selected) file_flags |= ImGuiTreeNodeFlags_Selected;
                    ImGui::TreeNodeEx(entry.display_name.c_str(), file_flags);

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                        m_State.SetSelectedAsset(entry.path, entry.kind);
                        if (entry.kind == EditorAssetKind::Unknown) {
                            m_AssetBrowserStatus = std::format("Unsupported asset type: {}", entry.relative_path);
                        } else {
                            m_AssetBrowserStatus.clear();
                        }
                    }

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", entry.relative_path.c_str());
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            if (IsPreviewableAsset(entry)) {
                                PreviewAssetFromBrowser(entry);
                            } else {
                                m_AssetBrowserStatus = std::format("Unsupported asset type: {}", entry.relative_path);
                            }
                        }
                    }

                    if (ImGui::BeginDragDropSource()) {
                        const auto payload = entry.path.string();
                        ImGui::SetDragDropPayload("HITAGI_ASSET_PATH", payload.c_str(), payload.size() + 1);
                        ImGui::Text("%s", entry.relative_path.c_str());
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        m_State.SetSelectedAsset(entry.path, entry.kind);
                    }
                    if (ImGui::BeginPopupContextItem("AssetContextMenu")) {
                        const auto can_preview = IsPreviewableAsset(entry);
                        if (!can_preview) ImGui::BeginDisabled(true);
                        if (ImGui::MenuItem("Preview")) PreviewAssetFromBrowser(entry);
                        if (!can_preview) ImGui::EndDisabled();

                        const auto can_import = IsImportableAsset(entry);
                        if (!can_import) ImGui::BeginDisabled(true);
                        if (ImGui::MenuItem("Import")) ImportAssetFromBrowser(entry);
                        if (!can_import) ImGui::EndDisabled();

                        ImGui::Separator();
                        ImGui::TextDisabled("%s", EditorAssetKindName(entry.kind).data());
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                    i = next;
                }
            };
            draw_tree(0, m_FilteredAssetIndices.size(), 0);
        }
        ImGui::EndChild();
    }
    ImGui::End();
    if (!open) m_State.SetPanelVisible(EditorPanel::AssetExplorer, false);
}
}  // namespace hitagi
