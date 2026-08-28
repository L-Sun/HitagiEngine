module;

#include <tracy/Tracy.hpp>

export module editor:asset_browser;
import engine;
import :state;

export namespace hitagi {
struct EditorAssetBrowserEntry {
    std::filesystem::path path;
    std::pmr::string      relative_path;
    std::pmr::string      display_name;
    EditorAssetKind       kind = EditorAssetKind::Unknown;
};

class EditorAssetBrowserModel {
public:
    void SetRoot(std::filesystem::path root) {
        if (m_Root == root) return;
        m_Root             = std::move(root);
        m_RefreshRequested = true;
    }

    auto GetRoot() const noexcept -> const std::filesystem::path& { return m_Root; }
    auto GetEntries() const noexcept -> const std::pmr::vector<EditorAssetBrowserEntry>& { return m_Entries; }
    auto GetRevision() const noexcept -> std::uint64_t { return m_Revision; }
    auto GetLastError() const noexcept -> std::string_view { return m_LastError; }
    auto NeedsRefresh() const noexcept -> bool { return m_RefreshRequested; }

    void RequestRefresh() noexcept { m_RefreshRequested = true; }

    template <typename Classifier>
    void Refresh(Classifier&& classifier) {
        ZoneScopedN("AssetBrowserModel::Refresh");

        m_RefreshRequested = false;
        m_LastError.clear();
        m_Entries.clear();

        std::error_code ec;
        if (!std::filesystem::exists(m_Root, ec) || !std::filesystem::is_directory(m_Root, ec)) {
            m_LastError = "Asset root is missing or is not a directory.";
            ++m_Revision;
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 m_Root,
                 std::filesystem::directory_options::skip_permission_denied,
                 ec)) {
            if (ec) {
                m_LastError = std::format("Asset browser error: {}", ec.message());
                break;
            }
            if (!entry.is_regular_file(ec)) continue;

            const auto path = entry.path();

            std::error_code relative_ec;
            auto            relative_path = std::filesystem::relative(path, m_Root, relative_ec);
            if (relative_ec) relative_path = path.filename();

            const auto relative_text = relative_path.generic_string();
            const auto display_name  = path.filename().string();
            m_Entries.emplace_back(EditorAssetBrowserEntry{
                .path          = path,
                .relative_path = relative_text.c_str(),
                .display_name  = display_name.c_str(),
                .kind          = classifier(path),
            });
        }

        std::ranges::sort(m_Entries, {}, &EditorAssetBrowserEntry::relative_path);
        ++m_Revision;
    }

private:
    std::filesystem::path                     m_Root;
    std::pmr::vector<EditorAssetBrowserEntry> m_Entries;
    std::pmr::string                          m_LastError;
    std::uint64_t                             m_Revision         = 0;
    bool                                      m_RefreshRequested = true;
};

class EditorAssetBrowserController {
public:
    explicit EditorAssetBrowserController(EditorAssetBrowserModel& model) noexcept : m_Model(model) {}

    void SetRoot(std::filesystem::path root) { m_Model.SetRoot(std::move(root)); }
    void RequestRefresh() noexcept { m_Model.RequestRefresh(); }

    template <typename Classifier>
    void RefreshIfNeeded(Classifier&& classifier) {
        if (m_Model.NeedsRefresh()) {
            m_Model.Refresh(std::forward<Classifier>(classifier));
        }
    }

private:
    EditorAssetBrowserModel& m_Model;
};
}  // namespace hitagi
