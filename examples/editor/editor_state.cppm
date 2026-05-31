module;

export module editor:state;
import engine;

export namespace hitagi {
enum struct EditorMode : std::uint8_t {
    Edit,
    Play,
    Pause,
};

enum struct EditorTool : std::uint8_t {
    Select,
    Translate,
    Rotate,
    Scale,
};

enum struct EditorCoordinateSpace : std::uint8_t {
    Local,
    World,
};

enum struct EditorPanel : std::uint8_t {
    SceneGraph,
    SceneNodeModifier,
    SceneViewer,
    AssetExplorer,
    DebugProfiling,
};

enum struct EditorAssetKind : std::uint8_t {
    Unknown,
    Scene,
    Model,
    Texture,
    Material,
};

class EditorState {
public:
    EditorState() {
        m_PanelVisibility = utils::create_enum_array<bool, EditorPanel>(true);
    }

    void SetCurrentScene(std::shared_ptr<asset::Scene> scene) noexcept {
        m_CurrentScene = std::move(scene);
        m_SelectedEntity = {};
    }

    auto GetCurrentScene() const noexcept -> const std::shared_ptr<asset::Scene>& { return m_CurrentScene; }

    void SelectEntity(ecs::Entity entity) noexcept { m_SelectedEntity = entity; }
    void ClearSelection() noexcept { m_SelectedEntity = {}; }
    auto GetSelectedEntity() const noexcept -> ecs::Entity { return m_SelectedEntity; }

    void MarkDirty() noexcept { ++m_DirtyRevision; }
    void MarkClean() noexcept { m_CleanRevision = m_DirtyRevision; }
    auto IsDirty() const noexcept -> bool { return m_DirtyRevision != m_CleanRevision; }
    auto GetDirtyRevision() const noexcept -> std::uint64_t { return m_DirtyRevision; }

    void SetMode(EditorMode mode) noexcept { m_Mode = mode; }
    auto GetMode() const noexcept -> EditorMode { return m_Mode; }

    void SetActiveTool(EditorTool tool) noexcept { m_ActiveTool = tool; }
    auto GetActiveTool() const noexcept -> EditorTool { return m_ActiveTool; }

    void SetCoordinateSpace(EditorCoordinateSpace space) noexcept { m_CoordinateSpace = space; }
    auto GetCoordinateSpace() const noexcept -> EditorCoordinateSpace { return m_CoordinateSpace; }

    void SetSnapEnabled(bool enabled) noexcept { m_SnapEnabled = enabled; }
    auto IsSnapEnabled() const noexcept -> bool { return m_SnapEnabled; }
    void SetTranslateSnap(float value) noexcept { m_TranslateSnap = value; }
    auto GetTranslateSnap() const noexcept -> float { return m_TranslateSnap; }
    void SetRotateSnap(float value) noexcept { m_RotateSnap = value; }
    auto GetRotateSnap() const noexcept -> float { return m_RotateSnap; }
    void SetScaleSnap(float value) noexcept { m_ScaleSnap = value; }
    auto GetScaleSnap() const noexcept -> float { return m_ScaleSnap; }

    void SetPanelVisible(EditorPanel panel, bool visible) noexcept { m_PanelVisibility[panel] = visible; }
    auto IsPanelVisible(EditorPanel panel) const noexcept -> bool { return m_PanelVisibility[panel]; }

    void SetSelectedAsset(std::filesystem::path path, EditorAssetKind kind) {
        m_SelectedAssetPath = std::move(path);
        m_SelectedAssetKind = kind;
    }
    void ClearSelectedAsset() {
        m_SelectedAssetPath.clear();
        m_SelectedAssetKind = EditorAssetKind::Unknown;
    }
    auto GetSelectedAssetPath() const noexcept -> const std::filesystem::path& { return m_SelectedAssetPath; }
    auto GetSelectedAssetKind() const noexcept -> EditorAssetKind { return m_SelectedAssetKind; }

private:
    std::shared_ptr<asset::Scene> m_CurrentScene = nullptr;
    ecs::Entity                   m_SelectedEntity;

    std::uint64_t m_DirtyRevision = 0;
    std::uint64_t m_CleanRevision = 0;

    EditorMode            m_Mode            = EditorMode::Edit;
    EditorTool            m_ActiveTool      = EditorTool::Select;
    EditorCoordinateSpace m_CoordinateSpace = EditorCoordinateSpace::Local;
    bool                  m_SnapEnabled     = false;
    float                 m_TranslateSnap   = 1.0f;
    float                 m_RotateSnap      = 15.0_deg;
    float                 m_ScaleSnap       = 0.1f;

    utils::EnumArray<bool, EditorPanel> m_PanelVisibility{};

    std::filesystem::path m_SelectedAssetPath;
    EditorAssetKind       m_SelectedAssetKind = EditorAssetKind::Unknown;
};
}  // namespace hitagi
