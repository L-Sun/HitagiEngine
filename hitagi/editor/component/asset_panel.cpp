module;

#include "interop/tracy_macros.hpp"

module editor;
import std;
import interop.tracy;
import interop.magic_enum;

using namespace hitagi::math;
using namespace hitagi::asset;
using namespace std::literals;

namespace hitagi {
constexpr auto kMaxTextPreviewBytes = 1024u * 1024u;

auto ContainsCaseInsensitive(std::string_view text, std::string_view filter) -> bool {
    if (filter.empty()) return true;
    if (filter.size() > text.size()) return false;
    for (std::size_t i = 0; i + filter.size() <= text.size(); ++i) {
        bool matched = true;
        for (std::size_t j = 0; j < filter.size(); ++j) {
            if (std::tolower(static_cast<unsigned char>(text[i + j])) != std::tolower(static_cast<unsigned char>(filter[j]))) {
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
        auto& asset_manager = m_Engine.Assets();
        m_State.SetSelectedAsset(entry.path, entry.kind);
        m_LastImportedAssetPath.clear();
        m_LastImportedAssetUUID.clear();

        if (entry.kind == EditorAssetKind::Scene || entry.kind == EditorAssetKind::Model) {
            m_CookContext.Clear();
            auto scene = ImportEditorScene(
                asset_manager,
                entry.path,
                entry.path.parent_path(),
                [&asset_manager](std::string_view name) { return asset_manager.GetMaterial(name); },
                nullptr,
                m_LaunchOptions.material_processor,
                std::addressof(m_CookContext));
            if (scene) {
                asset_manager.AddScene(scene);
                m_LastImportedAssetPath = entry.path;
                m_LastImportedAssetUUID = std::format("{}", scene->GetUUID());
                SetCurrentScene(std::move(scene));
                m_CurrentScenePath   = entry.path;
                m_AssetBrowserStatus = std::format("Imported scene: {}", entry.relative_path);
                m_AssetBrowserModel.RequestRefresh();
                Notify(m_AssetBrowserStatus);
            }
        } else if (entry.kind == EditorAssetKind::Texture) {
            auto texture = asset_manager.ImportTexture(entry.path);
            if (texture) {
                texture->Load(m_Engine.ResourceLoadContext());
                m_SelectedTexturePreview = texture;
                m_ImageViewer->SetTexture(texture);
                m_LastImportedAssetPath = entry.path;
                m_LastImportedAssetUUID = std::format("{}", texture->GetUUID());
                m_AssetBrowserStatus    = std::format("Previewing texture: {}", entry.relative_path);
                Notify(m_AssetBrowserStatus);
            }
        } else if (entry.kind == EditorAssetKind::Material) {
            auto material = asset_manager.ImportMaterial(entry.path);
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
    m_ImageViewer->SetTexture();
    m_AssetPreviewText.clear();
    m_AssetPreviewStatus.clear();
    m_State.SetPanelVisible(EditorPanel::AssetPreview, true);

    try {
        if (entry.kind == EditorAssetKind::Texture) {
            auto texture = m_Engine.Assets().ImportTexture(entry.path);
            if (texture) {
                texture->Load(m_Engine.ResourceLoadContext());
                m_SelectedTexturePreview = std::move(texture);
                m_ImageViewer->SetTexture(m_SelectedTexturePreview);
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

}  // namespace hitagi
