module;

#include "imfilebrowser.hpp"
#include <spdlog/logger.h>

export module editor;
export import :state;
export import :command;
export import :asset_browser;
export import :cook;
export import :usd;
export import :scene_viewport;
export import :image_viewer;
export import :material_compiler;
import engine;

export namespace hitagi {
struct EditorLaunchOptions {
    std::optional<std::filesystem::path> open_scene;
    std::optional<std::uint64_t>         frames;
    std::optional<std::filesystem::path> screenshot;
    std::optional<std::filesystem::path> hid_script;
    std::optional<std::filesystem::path> hid_control;
    UsdSceneImporter::MaterialProcessor  material_processor;
    bool                                 exit_after_load = false;
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

enum struct EditorFileDialogMode : std::uint8_t {
    None,
    OpenScene,
    ImportScene,
};

class Editor : public core::RuntimeModule {
public:
    Editor(Engine& engine);
    Editor(Engine& engine, EditorLaunchOptions options);
    void Tick() final;

private:
    void HandleShortcuts();
    void ApplyEditorStyle();
    auto DrawDockSpace() -> ImGuiID;
    void PrepareDefaultLayout(ImGuiID dockspace_id);
    auto HasSavedDockLayout() const -> bool;
    void MenuBar();
    void Toolbar();
    void FileImporter();
    void SceneGraphViewer();
    void SceneNodeModifier();
    void AssetExplorer();
    void AssetPreview();
    void DebugProfilingPanel();
    void RefreshAssetBrowser();
    void ImportAssetFromBrowser(const EditorAssetBrowserEntry& entry);
    void PreviewAssetFromBrowser(const EditorAssetBrowserEntry& entry);
    void DrawAssetPreview();
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
    EditorState                   m_State;
    EditorCookContext             m_CookContext;
    EditorCommandStack            m_CommandStack;
    std::shared_ptr<asset::Scene> m_EditScene;
    std::shared_ptr<asset::Scene> m_RuntimeScene;
    bool                          m_RuntimeStepRequested = false;

    core::Clock          m_Clock;
    SceneViewPort*       m_SceneViewPort = nullptr;
    ImageViewer*         m_ImageViewer   = nullptr;
    ImGui::FileBrowser   m_FileDialog;
    EditorFileDialogMode m_FileDialogMode = EditorFileDialogMode::None;

    std::uint64_t                             m_FrameIndex = 0;
    std::pmr::vector<EditorSyntheticHidEvent> m_SyntheticHidEvents;
    std::size_t                               m_NextSyntheticHidEvent = 0;
    std::uintmax_t                            m_HidControlOffset      = 0;
    bool                                      m_ScreenshotQueued      = false;
    std::shared_ptr<gfx::GPUBuffer>           m_ScreenshotBuffer      = nullptr;
    std::uint32_t                             m_ScreenshotWidth       = 0;
    std::uint32_t                             m_ScreenshotHeight      = 0;

    std::array<char, 128>              m_AssetSearchBuffer{};
    EditorAssetBrowserModel            m_AssetBrowserModel;
    std::pmr::vector<std::size_t>      m_FilteredAssetIndices;
    std::pmr::string                   m_AssetFilterCache;
    std::uint64_t                      m_AssetFilterRevision = std::numeric_limits<std::uint64_t>::max();
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
    bool                               m_StyleApplied      = false;
    bool                               m_DefaultLayoutDone = false;
};

}  // namespace hitagi
