module;

#include "interop/tracy_macros.hpp"

module editor;
import interop.nlohmann_json;
import interop.spdlog;
import interop.tracy;
import interop.magic_enum;

import std;

using namespace hitagi::math;
using namespace hitagi::asset;
using namespace std::literals;

namespace hitagi {
constexpr auto AsciiLower(char value) noexcept -> char {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

auto IEqualsAscii(std::string_view lhs, std::string_view rhs) noexcept -> bool {
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (AsciiLower(lhs[i]) != AsciiLower(rhs[i])) return false;
    }
    return true;
}

auto LowerAscii(std::string_view value) -> std::string {
    std::string result(value);
    for (auto& ch : result) ch = AsciiLower(ch);
    return result;
}

auto ParseSyntheticKeyName(std::string_view name) -> std::optional<hid::VirtualKeyCode> {
    if (auto key = magic_enum::enum_cast<hid::VirtualKeyCode>(name)) return key;

    if (name.size() == 1) {
        const auto ch = static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
        if (ch >= 'A' && ch <= 'Z') {
            return magic_enum::enum_cast<hid::VirtualKeyCode>(std::format("KEY_{}", ch));
        }
        if (ch >= '0' && ch <= '9') {
            return magic_enum::enum_cast<hid::VirtualKeyCode>(std::format("KEY_{}", ch));
        }
    }

    if (name == "LMB") return hid::VirtualKeyCode::MOUSE_L_BUTTON;
    if (name == "RMB") return hid::VirtualKeyCode::MOUSE_R_BUTTON;
    if (name == "MMB") return hid::VirtualKeyCode::MOUSE_M_BUTTON;
    if (name == "ALT" || name == "LALT" || name == "KEY_L_ALT") return hid::VirtualKeyCode::KEY_L_ALT;
    if (name == "RALT" || name == "KEY_R_ALT") return hid::VirtualKeyCode::KEY_R_ALT;
    if (name == "SHIFT" || name == "LSHIFT" || name == "KEY_L_SHIFT") return hid::VirtualKeyCode::KEY_L_SHIFT;
    if (name == "RSHIFT" || name == "KEY_R_SHIFT") return hid::VirtualKeyCode::KEY_R_SHIFT;
    return std::nullopt;
}

auto ParseSyntheticHidEventJson(const nlohmann::json& row) -> std::optional<EditorSyntheticHidEvent> {
    if (!row.is_object()) return std::nullopt;

    EditorSyntheticHidEvent event;
    event.frame = row.value("frame", std::uint64_t{0});

    const auto type = row.value("type", std::string{});
    if (row.contains("key") || type == "key") {
        const auto key_name = row.value("key", std::string{});
        const auto key      = ParseSyntheticKeyName(key_name);
        if (!key) {
            spdlog::warn("Editor HID skipped unknown key: {}", key_name);
            return std::nullopt;
        }
        event.kind = EditorSyntheticHidEventKind::Key;
        event.key  = *key;
        event.down = row.value("down", true);
    } else if (row.contains("pointer") || type == "pointer" || type == "mouse_move") {
        const auto& pointer = row.contains("pointer") ? row.at("pointer") : row;
        event.kind          = EditorSyntheticHidEventKind::Pointer;
        event.value         = {
            pointer.value("x", 0.0f),
            pointer.value("y", 0.0f),
        };
    } else if (row.contains("wheel") || type == "wheel") {
        const auto& wheel = row.contains("wheel") ? row.at("wheel") : row;
        event.kind        = EditorSyntheticHidEventKind::Wheel;
        event.value       = {
            wheel.value("x", 0.0f),
            wheel.value("y", 0.0f),
        };
    } else {
        spdlog::warn("Editor HID skipped event without key, pointer, or wheel.");
        return std::nullopt;
    }

    return event;
}

auto ParseSyntheticHidEvents(const std::filesystem::path& path) -> std::pmr::vector<EditorSyntheticHidEvent> {
    std::pmr::vector<EditorSyntheticHidEvent> events;

    std::ifstream input(path);
    if (!input) {
        spdlog::warn("Editor HID script can not be opened: {}", path.string());
        return events;
    }

    nlohmann::json json;
    try {
        input >> json;
    } catch (const nlohmann::json::exception& error) {
        spdlog::warn("Editor HID script parse failed: {} ({})", path.string(), error.what());
        return events;
    }

    const auto rows = json.is_object() && json.contains("events") ? json.at("events") : json;
    if (!rows.is_array()) {
        spdlog::warn("Editor HID script must be a JSON array or an object with an events array: {}", path.string());
        return events;
    }

    for (const auto& row : rows) {
        if (auto event = ParseSyntheticHidEventJson(row)) {
            events.emplace_back(*event);
        }
    }

    std::ranges::sort(events, {}, &EditorSyntheticHidEvent::frame);
    return events;
}

auto GetEditorConfigPath() -> std::filesystem::path {
    return "editor.json";
}

auto LoadAppConfigJson(const nlohmann::json& json) -> AppConfig {
    AppConfig defaults;
    AppConfig config;

    config.title       = json.value("title", std::string(defaults.title));
    config.version     = json.value("version", std::string(defaults.version));
    config.width       = json.value("width", defaults.width);
    config.height      = json.value("height", defaults.height);
    config.maximized   = json.value("maximized", defaults.maximized);
    config.gfx_backend = json.value("gfx_backend", std::string(defaults.gfx_backend));
    config.log_level   = json.value("log_level", std::string(defaults.log_level));
    config.headless    = json.value("headless", defaults.headless);
    if (json.contains("asset_root_path")) {
        config.asset_root_path = json.at("asset_root_path").get<std::filesystem::path>();
    } else {
        config.asset_root_path = defaults.asset_root_path;
    }

    return config;
}

auto LoadEditorAppConfig(const std::filesystem::path& path) -> AppConfig {
    auto load_from = [](const std::filesystem::path& config_path) -> std::optional<AppConfig> {
        if (!std::filesystem::exists(config_path)) return std::nullopt;

        std::ifstream input(config_path);
        if (!input) return std::nullopt;

        try {
            nlohmann::json json;
            input >> json;
            return LoadAppConfigJson(json);
        } catch (const nlohmann::json::exception& error) {
            spdlog::warn("Editor config parse failed: {} ({})", config_path.string(), error.what());
            return std::nullopt;
        }
    };

    if (auto config = load_from(path)) return *config;
    return {};
}

void SaveEditorAppConfig(const AppConfig& config, const std::filesystem::path& path) {
    const nlohmann::json json{
        {"title", std::string(config.title)},
        {"version", std::string(config.version)},
        {"width", config.width},
        {"height", config.height},
        {"maximized", config.maximized},
        {"asset_root_path", config.asset_root_path},
        {"gfx_backend", std::string(config.gfx_backend)},
        {"log_level", std::string(config.log_level)},
    };

    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        spdlog::warn("Editor config can not be written: {}", path.string());
        return;
    }
    output << json.dump(4);
}

auto ParseEditorLaunchOptions(int argc, const char* const* argv) -> EditorLaunchOptions {
    EditorLaunchOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        const auto             read_value = [&]() -> std::optional<std::string_view> {
            if (i + 1 >= argc) return std::nullopt;
            return std::string_view{argv[++i]};
        };

        if (arg == "--headless") {
            options.headless = true;
        } else if (arg == "--gfx-backend") {
            if (auto value = read_value()) options.gfx_backend = *value;
        } else if (arg == "--open-scene") {
            if (auto value = read_value()) options.open_scene = std::filesystem::path{*value};
        } else if (arg == "--frames") {
            if (auto value = read_value()) {
                try {
                    options.frames = std::stoull(std::string{*value});
                } catch (const std::exception&) {
                    spdlog::warn("Editor ignored invalid --frames value: {}", *value);
                }
            }
        } else if (arg == "--screenshot") {
            if (auto value = read_value()) options.screenshot = std::filesystem::path{*value};
        } else if (arg == "--hid-script") {
            if (auto value = read_value()) options.hid_script = std::filesystem::path{*value};
        } else if (arg == "--hid-control") {
            if (auto value = read_value()) options.hid_control = std::filesystem::path{*value};
        } else if (arg == "--exit-after-load") {
            options.exit_after_load = true;
        }
    }

    return options;
}

auto ClassifyEditorAssetPath(const std::filesystem::path& path) noexcept -> EditorAssetKind {
    const auto extension = LowerAscii(path.extension().string());
    if (asset::create_image_codec_for(extension) != nullptr) return EditorAssetKind::Texture;

    if (IsEditorScenePath(path)) return EditorAssetKind::Scene;

    if (IEqualsAscii(extension, ".json")) return EditorAssetKind::Material;
    if (IEqualsAscii(extension, ".slang") ||
        IEqualsAscii(extension, ".glsl") ||
        IEqualsAscii(extension, ".vert") ||
        IEqualsAscii(extension, ".frag") ||
        IEqualsAscii(extension, ".comp") ||
        IEqualsAscii(extension, ".wgsl")) {
        return EditorAssetKind::Shader;
    }
    return EditorAssetKind::Unknown;
}

auto EditorAssetKindName(EditorAssetKind kind) noexcept -> std::string_view {
    switch (kind) {
        case EditorAssetKind::Scene:
            return "Scene";
        case EditorAssetKind::Model:
            return "Model";
        case EditorAssetKind::Texture:
            return "Texture";
        case EditorAssetKind::Material:
            return "Material";
        case EditorAssetKind::Shader:
            return "Shader";
        default:
            return "Unsupported";
    }
}

auto EditorModeName(EditorMode mode) noexcept -> std::string_view {
    switch (mode) {
        case EditorMode::Edit:
            return "Edit";
        case EditorMode::Play:
            return "Play";
        case EditorMode::Pause:
            return "Pause";
    }
    return "Unknown";
}

auto CreateEditorDefaultCameraParameters() noexcept -> asset::Camera::Parameters {
    constexpr auto eye = math::vec3f{2.0f, 2.0f, 2.0f};
    return asset::Camera::Parameters{
        .aspect         = 16.0f / 9.0f,
        .near_clip      = 0.1f,
        .far_clip       = 100.0f,
        .horizontal_fov = 60.0_deg,
        .eye            = eye,
        .look_dir       = math::normalize(-eye),
        .up             = {0.0f, 0.0f, 1.0f},
    };
}

auto CreateEditorDefaultScene(std::string_view name) -> std::shared_ptr<asset::Scene> {
    auto scene = std::make_shared<asset::Scene>(name);
    scene->CreateCameraEntity(
        std::make_shared<asset::Camera>(CreateEditorDefaultCameraParameters(), "editor-camera"),
        math::mat4f::identity(),
        scene->GetRootEntity(),
        "camera");
    scene->Update();
    return scene;
}

auto CreateEditorFixtureScene(std::string_view name) -> std::shared_ptr<asset::Scene> {
    auto scene = std::make_shared<asset::Scene>(name);

    const auto parent = scene->CreateEmptyEntity(math::translate(math::vec3f{1.0f, 0.0f, 0.0f}), scene->GetRootEntity(), "parent");
    scene->CreateEmptyEntity(math::translate(math::vec3f{0.0f, 2.0f, 0.0f}), parent, "child");

    auto camera = std::make_shared<asset::Camera>(
        asset::Camera::Parameters{
            .aspect         = 16.0f / 9.0f,
            .near_clip      = 0.1f,
            .far_clip       = 100.0f,
            .horizontal_fov = 60.0_deg,
            .eye            = {0.0f, -4.0f, 2.0f},
            .look_dir       = {0.0f, 1.0f, -0.35f},
            .up             = {0.0f, 0.0f, 1.0f},
        },
        "fixture-camera");
    scene->CreateCameraEntity(camera, math::mat4f::identity(), scene->GetRootEntity(), "camera");

    auto light = std::make_shared<asset::Light>(
        asset::Light::Parameters{
            .type      = asset::Light::Type::Point,
            .intensity = 3.0f,
            .color     = math::Color::White(),
            .position  = {2.0f, -3.0f, 4.0f},
        },
        "fixture-light");
    scene->CreateLightEntity(light, math::translate(math::vec3f{2.0f, -3.0f, 4.0f}), scene->GetRootEntity(), "light");

    scene->Update();
    return scene;
}

auto CountSceneEntities(const asset::Scene& scene) -> std::size_t {
    std::size_t count = 0;

    std::function<void(ecs::Entity)> visit = [&](ecs::Entity entity) {
        if (!entity) return;
        ++count;
        if (!entity.Has<asset::RelationShip>()) return;

        for (const auto child : entity.Get<asset::RelationShip>().GetChildren()) {
            visit(child);
        }
    };

    visit(const_cast<asset::Scene&>(scene).GetRootEntity());
    return count;
}

auto CloneEditorEntitySubtree(asset::Scene& target_scene, ecs::Entity source, ecs::Entity target_parent) -> ecs::Entity {
    const auto name      = source.Has<asset::MetaInfo>()
                               ? std::string_view{source.Get<asset::MetaInfo>().name}
                               : std::string_view{"Entity"};
    const auto transform = source.Has<asset::Transform>()
                               ? source.Get<asset::Transform>().ToMatrix()
                               : math::mat4f::identity();

    ecs::Entity cloned;
    if (source.Has<asset::MeshComponent>()) {
        cloned = target_scene.CreateMeshEntity(source.Get<asset::MeshComponent>().mesh, transform, target_parent, name);
    } else if (source.Has<asset::CameraComponent>()) {
        cloned = target_scene.CreateCameraEntity(source.Get<asset::CameraComponent>().camera, transform, target_parent, name);
    } else if (source.Has<asset::LightComponent>()) {
        cloned = target_scene.CreateLightEntity(source.Get<asset::LightComponent>().light, transform, target_parent, name);
    } else {
        cloned = target_scene.CreateEmptyEntity(transform, target_parent, name);
    }

    if (source.Has<asset::RelationShip>()) {
        for (const auto child : source.Get<asset::RelationShip>().GetChildren()) {
            CloneEditorEntitySubtree(target_scene, child, cloned);
        }
    }
    return cloned;
}

auto CreateEditorRuntimeScene(const asset::Scene& edit_scene) -> std::shared_ptr<asset::Scene> {
    auto runtime_scene = std::make_shared<asset::Scene>(std::format("{} Runtime", edit_scene.GetName()));
    if (const auto root = edit_scene.GetRootEntity(); root && root.Has<asset::RelationShip>()) {
        for (const auto child : root.Get<asset::RelationShip>().GetChildren()) {
            CloneEditorEntitySubtree(*runtime_scene, child, runtime_scene->GetRootEntity());
        }
    }
    runtime_scene->Update();
    return runtime_scene;
}

Editor::Editor(Engine& engine) : Editor(engine, {}) {}

Editor::Editor(Engine& engine, EditorLaunchOptions options, std::unique_ptr<IEditorGamePreview> game)
    : core::RuntimeModule("Editor"), m_Engine(engine), m_App(engine.App()), m_LaunchOptions(std::move(options)), m_Game(std::move(game)) {
    m_Clock.Start();

    if (!m_Game) {
        m_SceneViewPort = static_cast<SceneViewPort*>(AddSubModule(std::make_unique<SceneViewPort>(engine, m_State, m_CommandStack)));
        m_ImageViewer = static_cast<ImageViewer*>(AddSubModule(std::make_unique<ImageViewer>(engine)));
    }
    m_Engine.GuiManager().LoadApp(m_LaunchOptions.ui_bundle, m_LaunchOptions.ui_assets, {.desktop = true, .raster_density = 2});
    if (m_Game) m_Game->Reset(false);
    else NewScene();
    m_AssetBrowserModel.SetRoot(m_App.GetConfig().asset_root_path);

    if (!m_Game && m_LaunchOptions.open_scene) {
        OpenScene(*m_LaunchOptions.open_scene);
    }
    if (m_LaunchOptions.hid_script) {
        LoadSyntheticHidScript(*m_LaunchOptions.hid_script);
    }
    if (m_LaunchOptions.hid_control) {
        const auto& path = *m_LaunchOptions.hid_control;
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        if (!std::filesystem::exists(path)) {
            std::ofstream{path};
        }
        m_HidControlOffset = std::filesystem::exists(path) ? std::filesystem::file_size(path) : 0;
        spdlog::info("Editor listening for synthetic HID JSONL at {}", path.string());
    }
}

Editor::~Editor() { UnloadAllSubModules(); }

void Editor::Tick() {
    ZoneScopedN("Editor::Tick");

    PollSyntheticHidControl();
    ApplySyntheticHidEvents();
    SavePendingScreenshot();

    if (m_State.GetCurrentScene()) {
        if (m_State.GetMode() == EditorMode::Edit || m_State.GetMode() == EditorMode::Play || m_RuntimeStepRequested) {
            ZoneScopedN("Editor Scene Update");
            m_State.GetCurrentScene()->Update(m_Engine.Jobs());
            m_RuntimeStepRequested = false;
        }
    }

    // Consume this frame's resized/dragged UI bounds before rendering native textures.
    if (m_Game) {
        const auto& input = m_App.GetInputManager();
        if (input.GetBoolNew(hid::VirtualKeyCode::MOUSE_L_BUTTON))
            m_GameFocused = m_Engine.GuiManager().PointerIn(m_GameRect);
        if (input.GetBoolNew(hid::VirtualKeyCode::KEY_ESCAPE)) m_GameFocused = false;
        m_Engine.GuiManager().SetKeyboardNavigationEnabled(!m_GameFocused);
    }
    m_Engine.GuiManager().PrepareFrame();
    HandleUiCommands();
    HandleShortcuts();
    if (!m_Game) RefreshAssetBrowser();
    PublishUiState();

    {
        ZoneScopedN("Editor Child Modules");
        if (m_Game) {
            auto& gui = m_Engine.GuiManager();
            const auto& input = m_App.GetInputManager();
            if (m_GameFocused && input.GetBoolNew(hid::VirtualKeyCode::KEY_SPACE)) TogglePauseMode();
            if (m_GameFocused && input.GetBoolNew(hid::VirtualKeyCode::KEY_R) && m_State.GetMode() != EditorMode::Edit) {
                m_Game->Reset(true);
                m_State.SetMode(EditorMode::Play);
            }
            m_Game->Update(m_Engine.GetDeltaTime().count(), m_GameFocused && m_State.GetMode() != EditorMode::Edit);
            m_Game->PublishState(gui, m_GameFocused);
            if (!m_App.WindowsMinimized() && m_GameRect.z > 0 && m_GameRect.w > 0) {
                const auto physical = gui.DisplayRect(m_GameRect);
                gui.DrawTexture(m_Game->Render({std::max(1u, static_cast<unsigned>(physical.z)),
                                                std::max(1u, static_cast<unsigned>(physical.w))}), m_GameRect);
            }
        } else core::RuntimeModule::Tick();
    }

    if (!m_App.WindowsMinimized()) {
        ZoneScopedN("Editor Render Output Setup");

        auto&      render_runtime = m_Engine.RenderRuntime();
        auto&      render_graph   = render_runtime.GetRenderGraph();
        const auto output         = render_graph.Create(
            {
                .name        = "Editor Output",
                .width       = m_App.GetWindowWidth(),
                .height      = m_App.GetWindowHeight(),
                .format      = gfx::Format::R8G8B8A8_UNORM,
                .clear_value = math::Color{0.0f, 0.0f, 0.0f, 1.0f},
                .usages      = gfx::TextureUsageFlags::CopySrc | gfx::TextureUsageFlags::RenderTarget,
            },
            "Editor Output");
        render_runtime.RenderGui(output, m_Engine.GuiManager().GetDrawData(), true);
        QueueScreenshot(output);
        render_runtime.ToSwapChain(output);
    }

    TickLaunchAutomation();

    m_Clock.Tick();
}

void Editor::OpenScene(const std::filesystem::path& path) {
    if (GetEditorSceneFormat(path.extension().string()) == EditorSceneFormat::Unknown) {
        spdlog::warn("Editor can not open unsupported scene format: {}", path.string());
        return;
    }

    auto& asset_manager = m_Engine.Assets();
    m_CookContext.Clear();
    auto scene = ImportEditorScene(
        asset_manager,
        path,
        path.parent_path(),
        [&asset_manager](std::string_view name) { return asset_manager.GetMaterial(name); },
        nullptr,
        m_LaunchOptions.material_processor,
        std::addressof(m_CookContext));
    if (scene) {
        asset_manager.AddScene(scene);
        SetCurrentScene(std::move(scene));
        m_CurrentScenePath = path;
        m_AssetBrowserModel.RequestRefresh();
        m_State.MarkClean();
        spdlog::info("Editor opened scene: {}", path.string());
        Notify(std::format("Opened scene: {}", path.string()));
    } else {
        spdlog::error("Editor failed to open scene: {}", path.string());
        Notify(std::format("Failed to open scene: {}", path.string()));
    }
}

void Editor::NewScene() {
    m_CookContext.Clear();
    SetCurrentScene(CreateEditorDefaultScene("Untitled"));
    m_CurrentScenePath.clear();
    m_State.MarkClean();
    Notify("Created new scene.");
}

void Editor::RequestOpenScene() {
    Notify("Select a scene in Assets and choose Open, or launch with --open-scene <path>.");
}

void Editor::RequestImportScene() { RequestOpenScene(); }

void Editor::RequestSaveSceneAs() {
    if (!m_State.GetCurrentScene()) return;
    Notify("Saving scenes is disabled until scene export is implemented.");
}

void Editor::SaveCurrentScene() {
    const auto scene_to_save = m_State.GetMode() == EditorMode::Edit ? m_State.GetCurrentScene() : m_EditScene;
    if (!scene_to_save) return;
    Notify("Saving scenes is disabled until scene export is implemented.");
}

void Editor::SetCurrentScene(std::shared_ptr<asset::Scene> scene) {
    ActivateScene(std::move(scene));
    m_CommandStack.Clear();
    m_EditScene.reset();
    m_RuntimeScene.reset();
    m_State.SetMode(EditorMode::Edit);
}

void Editor::ActivateScene(std::shared_ptr<asset::Scene> scene) {
    m_State.SetCurrentScene(std::move(scene));
    m_SceneViewPort->SetScene(m_State.GetCurrentScene());
}

void Editor::EnterPlayMode() {
    if (m_Game) {
        if (m_State.GetMode() != EditorMode::Edit) return;
        m_Game->Reset(true);
        m_State.SetMode(EditorMode::Play);
        Notify("Game started. Click the Game viewport to control it.");
        return;
    }
    if (m_State.GetMode() != EditorMode::Edit || !m_State.GetCurrentScene()) return;

    m_EditScene               = m_State.GetCurrentScene();
    m_EditScenePathBeforePlay = m_CurrentScenePath;
    m_RuntimeScene            = CreateEditorRuntimeScene(*m_EditScene);
    ActivateScene(m_RuntimeScene);
    m_State.SetMode(EditorMode::Play);
    Notify("Entered Play mode with an isolated runtime scene.");
}

void Editor::ExitPlayMode() {
    if (m_Game) {
        m_Game->Reset(false);
        m_GameFocused = false;
        m_State.SetMode(EditorMode::Edit);
        Notify("Stopped. Edit configuration restored.");
        return;
    }
    if (m_State.GetMode() == EditorMode::Edit) return;

    ActivateScene(m_EditScene);
    m_CurrentScenePath = m_EditScenePathBeforePlay;
    m_RuntimeScene.reset();
    m_EditScene.reset();
    m_RuntimeStepRequested = false;
    m_State.SetMode(EditorMode::Edit);
    Notify("Exited Play mode and restored the edit scene.");
}

void Editor::TogglePauseMode() {
    if (m_Game && m_State.GetMode() != EditorMode::Edit) m_Game->Pause(m_State.GetMode() == EditorMode::Play);
    if (m_State.GetMode() == EditorMode::Play) {
        m_State.SetMode(EditorMode::Pause);
        Notify("Play mode paused.");
    } else if (m_State.GetMode() == EditorMode::Pause) {
        m_State.SetMode(EditorMode::Play);
        Notify("Play mode resumed.");
    }
}

void Editor::StepPlayMode() {
    if (m_Game) {
        if (m_State.GetMode() == EditorMode::Edit) return;
        m_Game->Step();
        m_State.SetMode(EditorMode::Pause);
        Notify("Advanced one game logic step.");
        return;
    }
    if (m_State.GetMode() == EditorMode::Edit || !m_State.GetCurrentScene()) return;
    m_State.SetMode(EditorMode::Pause);
    m_RuntimeStepRequested = true;
    Notify("Play mode stepped one frame.");
}

void Editor::Undo() {
    if (m_State.GetMode() != EditorMode::Edit) return;
    if (!m_CommandStack.CanUndo()) return;
    m_CommandStack.Undo();
    if (m_CommandStack.IsDirty()) {
        m_State.MarkDirty();
    } else {
        m_State.MarkClean();
    }
}

void Editor::Redo() {
    if (m_State.GetMode() != EditorMode::Edit) return;
    if (!m_CommandStack.CanRedo()) return;
    m_CommandStack.Redo();
    m_State.MarkDirty();
}

void Editor::DuplicateSelectedEntity() {
    if (m_State.GetMode() != EditorMode::Edit) return;
    auto scene  = m_State.GetCurrentScene();
    auto entity = m_State.GetSelectedEntity();
    if (!scene || !entity || entity == scene->GetRootEntity()) return;
    ExecuteCommand(std::make_unique<DuplicateEntitySubtreeCommand>(*scene, entity, &m_State));
}

void Editor::DeleteSelectedEntity() {
    if (m_State.GetMode() != EditorMode::Edit) return;
    auto scene  = m_State.GetCurrentScene();
    auto entity = m_State.GetSelectedEntity();
    if (!scene || !entity || entity == scene->GetRootEntity()) return;
    ExecuteCommand(std::make_unique<DeleteEntitySubtreeCommand>(*scene, entity, &m_State));
}

void Editor::ExecuteCommand(std::unique_ptr<EditorCommand> command) {
    if (m_State.GetMode() != EditorMode::Edit) {
        Notify("Editing commands are disabled outside Edit mode.");
        return;
    }
    m_CommandStack.Execute(std::move(command));
    m_State.MarkDirty();
}

void Editor::Notify(std::string_view message) {
    m_EditorNotifications.emplace_back(message);
    if (m_EditorNotifications.size() > 100) {
        m_EditorNotifications.erase(m_EditorNotifications.begin());
    }
}

void Editor::LoadSyntheticHidScript(const std::filesystem::path& path) {
    m_SyntheticHidEvents    = ParseSyntheticHidEvents(path);
    m_NextSyntheticHidEvent = 0;
    if (!m_SyntheticHidEvents.empty()) {
        spdlog::info("Editor loaded {} synthetic HID events from {}", m_SyntheticHidEvents.size(), path.string());
    }
}

void Editor::PollSyntheticHidControl() {
    if (!m_LaunchOptions.hid_control) return;

    const auto& path = *m_LaunchOptions.hid_control;
    if (!std::filesystem::exists(path)) return;

    const auto size = std::filesystem::file_size(path);
    if (size < m_HidControlOffset) {
        m_HidControlOffset = 0;
    }
    if (size == m_HidControlOffset) return;

    std::ifstream input(path, std::ios::binary);
    if (!input) return;
    input.seekg(static_cast<std::streamoff>(m_HidControlOffset), std::ios::beg);

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) continue;

        try {
            auto json = nlohmann::json::parse(line);
            if (auto event = ParseSyntheticHidEventJson(json)) {
                if (!json.contains("frame")) {
                    event->frame = m_FrameIndex;
                }
                m_SyntheticHidEvents.emplace_back(*event);
            }
        } catch (const nlohmann::json::exception& error) {
            spdlog::warn("Editor HID control skipped invalid JSONL event: {} ({})", line, error.what());
        }
    }

    const std::streamoff offset = input.tellg();
    m_HidControlOffset          = offset < 0 ? size : static_cast<std::uintmax_t>(offset);
    std::ranges::sort(m_SyntheticHidEvents.begin() + static_cast<std::ptrdiff_t>(m_NextSyntheticHidEvent), m_SyntheticHidEvents.end(), {}, &EditorSyntheticHidEvent::frame);
}

void Editor::ApplySyntheticHidEvents() {
    auto& input = m_App.GetInputManager();
    while (m_NextSyntheticHidEvent < m_SyntheticHidEvents.size() &&
           m_SyntheticHidEvents[m_NextSyntheticHidEvent].frame <= m_FrameIndex) {
        const auto& event = m_SyntheticHidEvents[m_NextSyntheticHidEvent++];
        switch (event.kind) {
            case EditorSyntheticHidEventKind::Key:
                input.UpdateKeyState(event.key, event.down);
                break;
            case EditorSyntheticHidEventKind::Pointer:
                input.UpdatePointerState(event.value.x, event.value.y);
                break;
            case EditorSyntheticHidEventKind::Wheel:
                input.UpdateWheelState(event.value.x, event.value.y);
                break;
        }
    }
}

void Editor::QueueScreenshot(rg::TextureHandle output) {
    if (!m_LaunchOptions.screenshot || m_ScreenshotQueued) return;
    if (m_LaunchOptions.frames && m_FrameIndex + 1 < *m_LaunchOptions.frames) return;

    auto&       render_runtime = m_Engine.RenderRuntime();
    auto&       render_graph   = render_runtime.GetRenderGraph();
    const auto& desc           = render_graph.GetResourceDesc(output);
    if (desc.format != gfx::Format::R8G8B8A8_UNORM) {
        spdlog::warn("Editor screenshot skipped for unsupported format.");
        return;
    }

    m_ScreenshotWidth  = desc.width;
    m_ScreenshotHeight = desc.height;
    m_ScreenshotBuffer = hitagi::gfx::GPUBuffer::Create(render_graph.GetDevice(), {
                                                                                      .name   = "Editor Screenshot Readback",
                                                                                      .size   = sizeof(std::uint32_t) * static_cast<std::uint64_t>(m_ScreenshotWidth) * m_ScreenshotHeight,
                                                                                      .usages = gfx::GPUBufferUsageFlags::MapRead | gfx::GPUBufferUsageFlags::CopyDst,
                                                                                  });
    render_runtime.CopyToBuffer(output, m_ScreenshotBuffer);
    m_ScreenshotQueued = true;
}

void Editor::SavePendingScreenshot() {
    if (!m_LaunchOptions.screenshot || !m_ScreenshotQueued || !m_ScreenshotBuffer) return;

    if (!m_LaunchOptions.screenshot->parent_path().empty()) {
        std::filesystem::create_directories(m_LaunchOptions.screenshot->parent_path());
    }
    const auto     readback_view = hitagi::gfx::GPUBufferView::Create(m_Engine.RenderRuntime().GetRenderGraph().GetDevice(), m_Engine.RenderRuntime().GetRenderGraph().GetBindings(), {
                                                                                                                                                                                          .buffer        = m_ScreenshotBuffer,
                                                                                                                                                                                          .element_size  = sizeof(std::uint32_t),
                                                                                                                                                                                          .element_count = 0,
                                                                                                                                                                                      });
    const auto     readback      = readback_view->GetMappedSpan<const std::uint32_t>();
    asset::Texture image(
        m_ScreenshotWidth,
        m_ScreenshotHeight,
        gfx::Format::R8G8B8A8_UNORM,
        core::Buffer(
            static_cast<std::size_t>(m_ScreenshotBuffer->Size()),
            reinterpret_cast<const std::byte*>(readback.data())));

    if (const auto png = asset::PngEncoder{}.Encode(image); !png.Empty()) {
        m_Engine.FileIO().SaveBuffer(png, *m_LaunchOptions.screenshot);
        spdlog::info("Editor screenshot saved: {}", m_LaunchOptions.screenshot->string());
    } else {
        spdlog::error("Editor screenshot save failed: {}", m_LaunchOptions.screenshot->string());
    }

    m_LaunchOptions.screenshot.reset();
    m_App.Quit();
}

void Editor::TickLaunchAutomation() {
    ++m_FrameIndex;

    if (m_LaunchOptions.exit_after_load && m_LaunchOptions.open_scene && !m_LaunchOptions.screenshot) {
        m_App.Quit();
        return;
    }

    if (!m_LaunchOptions.frames || m_FrameIndex < *m_LaunchOptions.frames) return;
    if (m_LaunchOptions.screenshot) return;

    m_App.Quit();
}

void Editor::HandleShortcuts() {
    if (m_Engine.GuiManager().WantsTextInput()) return;
    const auto& input = m_App.GetInputManager();
    using Key = hid::VirtualKeyCode;
    if (m_Game) {
        if (input.GetBoolNew(Key::KEY_F5)) {
            if (m_State.GetMode() == EditorMode::Edit) EnterPlayMode(); else ExitPlayMode();
        }
        if (input.GetBoolNew(Key::KEY_F6)) TogglePauseMode();
        if (input.GetBoolNew(Key::KEY_F10)) StepPlayMode();
        return;
    }
    const auto ctrl = input.GetBool(Key::KEY_CTRL) || input.GetBool(Key::KEY_L_CTRL) || input.GetBool(Key::KEY_R_CTRL);
    const auto shift = input.GetBool(Key::KEY_SHIFT) || input.GetBool(Key::KEY_L_SHIFT) || input.GetBool(Key::KEY_R_SHIFT);

    if (ctrl && input.GetBoolNew(Key::KEY_N)) NewScene();
    if (ctrl && input.GetBoolNew(Key::KEY_O)) RequestOpenScene();
    if (ctrl && shift && input.GetBoolNew(Key::KEY_S))
        RequestSaveSceneAs();
    else if (ctrl && input.GetBoolNew(Key::KEY_S))
        SaveCurrentScene();
    if (ctrl && !shift && input.GetBoolNew(Key::KEY_Z)) Undo();
    if ((ctrl && input.GetBoolNew(Key::KEY_Y)) || (ctrl && shift && input.GetBoolNew(Key::KEY_Z))) Redo();
    if (ctrl && input.GetBoolNew(Key::KEY_D)) DuplicateSelectedEntity();
    if (input.GetBoolNew(Key::KEY_DELETE)) DeleteSelectedEntity();
    if (input.GetBoolNew(Key::KEY_F5)) {
        if (m_State.GetMode() == EditorMode::Edit)
            EnterPlayMode();
        else
            ExitPlayMode();
    }
    if (input.GetBoolNew(Key::KEY_F6)) TogglePauseMode();
    if (input.GetBoolNew(Key::KEY_F10)) StepPlayMode();
    if (input.GetBoolNew(Key::KEY_Q)) m_State.SetActiveTool(EditorTool::Select);
    if (input.GetBoolNew(Key::KEY_W)) m_State.SetActiveTool(EditorTool::Translate);
    if (input.GetBoolNew(Key::KEY_E)) m_State.SetActiveTool(EditorTool::Rotate);
    if (input.GetBoolNew(Key::KEY_R)) m_State.SetActiveTool(EditorTool::Scale);
    if (input.GetBoolNew(Key::KEY_L)) m_State.SetCoordinateSpace(EditorCoordinateSpace::Local);
    if (input.GetBoolNew(Key::KEY_G)) m_State.SetCoordinateSpace(EditorCoordinateSpace::World);
    if (input.GetBoolNew(Key::KEY_F) && m_State.GetSelectedEntity()) {
        if (m_SceneViewPort) m_SceneViewPort->FocusSelectedEntity();
        Notify("Focused selected entity.");
    }
}

}  // namespace hitagi
