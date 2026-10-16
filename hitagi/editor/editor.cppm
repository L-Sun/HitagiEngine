export module editor;
import std;
export import :state;
export import :command;
export import :asset_browser;
export import :cook;
export import :usd;
export import :scene_viewport;
export import :image_viewer;
export import :material_graph;
import engine;

export namespace hitagi {
struct EditorLaunchOptions {
    std::filesystem::path ui_bundle = "build/pocketjs/ui/hitagi-ui.js";
    std::filesystem::path ui_assets = "build/pocketjs/ui/hitagi-ui.pak";
    std::optional<std::filesystem::path> open_scene;
    std::optional<std::string>           gfx_backend;
    std::optional<std::uint64_t>         frames;
    std::optional<std::filesystem::path> screenshot;
    std::optional<std::filesystem::path> hid_script;
    std::optional<std::filesystem::path> hid_control;
    UsdSceneImporter::MaterialProcessor  material_processor;
    bool                                 exit_after_load = false;
    bool                                 headless = false;
};

enum struct EditorSyntheticHidEventKind : std::uint8_t {
    Key,
    Pointer,
    Wheel,
};

struct EditorSyntheticHidEvent {
    std::uint64_t               frame = 0;
    EditorSyntheticHidEventKind kind  = EditorSyntheticHidEventKind::Key;
    hid::VirtualKeyCode         key   = hid::VirtualKeyCode::NONE;
    bool                        down  = false;
    math::vec2f                 value = {};
};

auto ParseEditorLaunchOptions(int argc, const char* const* argv) -> EditorLaunchOptions;
auto GetEditorConfigPath() -> std::filesystem::path;
auto LoadEditorAppConfig(const std::filesystem::path& path = GetEditorConfigPath()) -> AppConfig;
void SaveEditorAppConfig(const AppConfig& config, const std::filesystem::path& path = GetEditorConfigPath());
auto CreateEditorDefaultCameraParameters() noexcept -> asset::Camera::Parameters;
auto CreateEditorDefaultScene(std::string_view name = "Untitled") -> std::shared_ptr<asset::Scene>;
auto CreateEditorFixtureScene(std::string_view name = "editor-fixture") -> std::shared_ptr<asset::Scene>;
auto CountSceneEntities(const asset::Scene& scene) -> std::size_t;
auto ClassifyEditorAssetPath(const std::filesystem::path& path) noexcept -> EditorAssetKind;
auto EditorAssetKindName(EditorAssetKind kind) noexcept -> std::string_view;
auto EditorModeName(EditorMode mode) noexcept -> std::string_view;
auto CreateEditorRuntimeScene(const asset::Scene& edit_scene) -> std::shared_ptr<asset::Scene>;

struct EditorRenderGraphDebugNode {
    std::uint64_t    handle = 0;
    std::pmr::string name;
    bool             resource = false;
};

struct EditorRenderGraphDebugSnapshot {
    std::pmr::vector<EditorRenderGraphDebugNode> passes;
    std::pmr::vector<EditorRenderGraphDebugNode> resources;
};

auto BuildEditorRenderGraphDebugSnapshot(std::string_view dot) -> EditorRenderGraphDebugSnapshot;


// Owned by Editor; the game-editor supplies the implementation. No game types
// enter the generic editor. Render returns a handle for this frame only.
class IEditorGamePreview {
public:
    virtual ~IEditorGamePreview() = default;
    virtual void Reset(bool running) = 0;
    virtual void Pause(bool paused) = 0;
    virtual void Step() = 0;
    virtual void Update(double elapsed, bool accept_input) = 0;
    virtual auto Render(math::vec2u size) -> rg::TextureHandle = 0;
    virtual void PublishState(gui::GuiManager& gui, bool focused) = 0;
    virtual void Configure(std::string_view field, int delta) = 0;
};

class Editor : public core::RuntimeModule {
public:
    Editor(Engine& engine);
    Editor(Engine& engine, EditorLaunchOptions options, std::unique_ptr<IEditorGamePreview> game = {});
    ~Editor() override;
    void Tick() final;

private:
    void HandleShortcuts();
    void HandleUiCommands();
    void PublishUiState();
    void RefreshAssetBrowser();
    void ImportAssetFromBrowser(const EditorAssetBrowserEntry& entry);
    void PreviewAssetFromBrowser(const EditorAssetBrowserEntry& entry);
    void NewScene();
    void OpenScene(const std::filesystem::path& path);
    void RequestOpenScene();
    void RequestImportScene();
    void RequestSaveSceneAs();
    void SaveCurrentScene();
    void SetCurrentScene(std::shared_ptr<asset::Scene> scene);
    void ActivateScene(std::shared_ptr<asset::Scene> scene);
    void EnterPlayMode();
    void ExitPlayMode();
    void TogglePauseMode();
    void StepPlayMode();
    void Undo();
    void Redo();
    void DuplicateSelectedEntity();
    void DeleteSelectedEntity();
    void ExecuteCommand(std::unique_ptr<EditorCommand> command);
    void Notify(std::string_view message);
    void QueueScreenshot(rg::TextureHandle output);
    void SavePendingScreenshot();
    void TickLaunchAutomation();
    void LoadSyntheticHidScript(const std::filesystem::path& path);
    void PollSyntheticHidControl();
    void ApplySyntheticHidEvents();

    Engine&                       m_Engine;
    Application&                  m_App;
    EditorLaunchOptions           m_LaunchOptions;
    std::unique_ptr<IEditorGamePreview> m_Game;
    math::vec4f m_GameRect{};
    bool m_GameFocused = false;
    EditorState                   m_State;
    EditorCookContext             m_CookContext;
    EditorCommandStack            m_CommandStack;
    std::shared_ptr<asset::Scene> m_EditScene;
    std::shared_ptr<asset::Scene> m_RuntimeScene;
    bool                          m_RuntimeStepRequested = false;

    core::Clock          m_Clock;
    SceneViewPort*       m_SceneViewPort = nullptr;
    ImageViewer*         m_ImageViewer   = nullptr;
    std::string m_LastUiState;

    std::uint64_t                             m_FrameIndex = 0;
    std::pmr::vector<EditorSyntheticHidEvent> m_SyntheticHidEvents;
    std::size_t                               m_NextSyntheticHidEvent = 0;
    std::uintmax_t                            m_HidControlOffset      = 0;
    bool                                      m_ScreenshotQueued      = false;
    std::shared_ptr<gfx::GPUBuffer>           m_ScreenshotBuffer      = nullptr;
    std::uint32_t                             m_ScreenshotWidth       = 0;
    std::uint32_t                             m_ScreenshotHeight      = 0;

    EditorAssetBrowserModel            m_AssetBrowserModel;
    std::filesystem::path              m_CurrentScenePath;
    std::filesystem::path              m_EditScenePathBeforePlay;
    std::filesystem::path              m_LastImportedAssetPath;
    std::filesystem::path              m_AssetPreviewPath;
    EditorAssetKind                    m_AssetPreviewKind = EditorAssetKind::Unknown;
    std::pmr::string                   m_LastImportedAssetUUID;
    std::pmr::string                   m_AssetBrowserStatus;
    std::pmr::string                   m_AssetPreviewText;
    std::pmr::string                   m_AssetPreviewStatus;
    std::shared_ptr<asset::Texture>    m_SelectedTexturePreview;
    std::pmr::vector<std::pmr::string> m_EditorNotifications;
};

}  // namespace hitagi
