#include "interop/tracy_macros.hpp"
import interop.tracy;
import ecs;
import engine;
import std;
import platform;
import core;
import gfx;
import asset;
import hid;
import viewer;
#ifdef _WIN32
import interop.win32;
#endif

namespace hitagi::examples::test {

struct RecordedFrame {
    std::string  state;
    core::Buffer png;
};

void Require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

void Write(const std::filesystem::path& path, const core::Buffer& data) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.GetData()), data.GetDataSize());
    if (!file) throw std::runtime_error("Cannot write test artifact");
}

class Session {
public:
    Session(core::Runtime& runtime, gfx::Device::Type backend, const std::filesystem::path& output, bool replay,
            std::vector<RecordedFrame>& tape, bool record_animation = false,
            const std::filesystem::path& record_scene = {}, RendererType renderer = RendererType::Pbr,
            bool deterministic = true, bool fixed_size = false, bool headless = true)
        : m_Output(output),
          m_Replay(replay),
          m_RecordAnimation(record_animation),
          m_FixedSize(fixed_size),
          m_Tape(tape),
          m_Runtime(runtime),
          m_App(
              platform::PlatformHost::Create({.surface = backend == gfx::Device::Type::Metal ? platform::Surface::Metal : platform::Surface::Vulkan,
                                              .title   = "USD Viewer E2E",
                                              .width   = fixed_size ? 1000u : 1500u,
                                              .height  = fixed_size ? 700u : 1050u,

                                              .headless = headless})),
          m_Trace(output / (replay ? "replay.jsonl" : "record.jsonl")) {
        m_Host.AddPlugin(gfx::GraphicsPlugin{.backend = backend});
        m_Host.AddPlugin(AssetPlugin{.files = runtime.IO(), .jobs = runtime.Jobs()});
        m_Graphics = &m_Host.GetWorld().GetResource<gfx::GraphicsContext>();
        m_Host.AddPlugin(USDViewerPlugin{
            .platform      = *m_App,
            .resources     = {.device = *m_Graphics->device, .queues = m_Graphics->queues, .bindings = *m_Graphics->bindings, .shader_compiler = m_Graphics->compiler},
            .files         = m_Runtime.IO(),
            .jobs          = m_Runtime.Jobs(),
            .assets        = m_Host.GetWorld().GetResource<asset::AssetService>(),
            .scene         = record_scene.empty() ? std::filesystem::path(record_animation ? "examples/usd-viewer/assets/skinned.usda" : "examples/usd-viewer/assets/studio.usda") : record_scene,
            .renderer      = renderer,
            .deterministic = deterministic,
        });
        m_Viewer = &m_Host.GetWorld().GetResource<USDViewer>();
        if (!m_FixedSize) {
            // Preserve a useful logical viewport on high-DPI stations.
            const float dpi = m_App->GetDpiRatio();
            m_App->ResizeWindow(static_cast<unsigned>(1000 * dpi), static_cast<unsigned>(700 * dpi));
        }
    }

    ~Session() { asset::Texture::DestroyDefaultTexture(); }

    void Step(std::string_view action, const std::function<void()>& apply = {}) {
        // No event pumping, sleeps, retries or elapsed-time-driven UI advancement.
        // The tape is the only source of input; the actual PocketJS/GPU path runs.
        if (apply) apply();
        m_App->GetInputState().AdvanceFrame();
        m_Host.TickFrame();
        const auto state = m_Viewer->Snapshot();
        auto       image = m_Viewer->Capture();
        auto       png   = asset::PngEncoder{}.Encode(image);
        const auto rect  = m_Viewer->Controls().at("viewport").rect;
        const auto dpi   = m_App->GetDpiRatio();
        const auto bytes = image.data.Span<const std::byte>();
        m_ViewportPixels.clear();
        for (unsigned y = std::ceil(rect.y * dpi); y < (rect.y + rect.w) * dpi - 1; ++y) {
            const auto start = (y * image.width + static_cast<unsigned>(std::ceil(rect.x * dpi))) * 4;
            const auto count = static_cast<unsigned>(rect.z * dpi - 2) * 4;
            Require(start + count <= bytes.size(), "Viewport lies outside the rendered frame");
            m_ViewportPixels.insert(m_ViewportPixels.end(), bytes.begin() + start,
                                    bytes.begin() + start + count);
        }
        const auto frame = m_Viewer->FrameIndex();
        const auto path  = m_Output / std::format("frame-{:03}.png", frame);
        m_Trace << "{\"tick\":" << frame << ",\"action\":\"" << action << "\",\"state\":" << state << "}\n";
        if (!m_Replay) {
            Write(path, png);
            if (!m_RecordAnimation) m_Tape.emplace_back(state, std::move(png));
        } else {
            Require(frame <= m_Tape.size(), "Replay produced an extra frame");
            const auto& expected = m_Tape[frame - 1];
            const auto actual_bytes = png.Span<const std::byte>();
            const auto expected_bytes = expected.png.Span<const std::byte>();
            bool same_bytes = actual_bytes.size() == expected_bytes.size();
            for (std::size_t i = 0; same_bytes && i < actual_bytes.size(); ++i)
                same_bytes = actual_bytes[i] == expected_bytes[i];
            if (state != expected.state || !same_bytes) {
                Write(m_Output / std::format("diverged-{:03}.png", frame), png);
                throw std::runtime_error(std::format("Replay diverged at tick {} ({})", frame, action));
            }
        }
    }

    void Click(std::string_view name, bool allow_disabled = false) {
        const auto found = m_Viewer->Controls().find(std::string(name));
        Require(found != m_Viewer->Controls().end(), std::format("Control not found: {}", name));
        Require(allow_disabled || found->second.enabled, std::format("Control disabled: {}", name));
        const auto r = found->second.rect;
        // Resolve the actual layout, then use the same input route as the native app.
        // Press and release are separate ticks; no direct C++ command invocation.
        Step(std::format("{}.down", name), [&] {
            const float dpi = m_App->GetDpiRatio();
            m_App->GetInputState().UpdatePointerState((r.x + r.z / 2) * dpi, (r.y + r.w / 2) * dpi);
            m_App->GetInputState().UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, true);
        });
        Step(std::format("{}.up", name),
             [&] { m_App->GetInputState().UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, false); });
    }

    auto CameraState() const -> std::string {
        const auto state = m_Viewer->Snapshot();
        return state.substr(state.find("\"eye\":"));
    }

    void CameraJourney() {
        auto&       input    = m_App->GetInputState();
        const auto  before   = CameraState();
        const auto  viewport = m_Viewer->Controls().at("viewport").rect;
        const float dpi      = m_App->GetDpiRatio();
        const float x        = (viewport.x + viewport.z / 2) * dpi;
        const float y        = (viewport.y + viewport.w / 2) * dpi;
        Step("viewport.pointer", [&] { input.UpdatePointerState(x, y); });
        Step("orbit.down", [&] { input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, true); });
        Require(CameraState() == before, "Press jumped the orbit camera");
        Step("orbit.move", [&] { input.UpdatePointerState(x + 50 * dpi, y + 20 * dpi); });
        Require(CameraState() != before, "Orbit did not change the camera");
        Step("orbit.up", [&] { input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, false); });
        const auto orbited = CameraState();
        Step("pan.down", [&] { input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_M_BUTTON, true); });
        Step("pan.move", [&] { input.UpdatePointerState(x + 80 * dpi, y + 40 * dpi); });
        Require(CameraState() != orbited, "Pan did not change the camera");
        Step("pan.up", [&] { input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_M_BUTTON, false); });
        const auto panned = CameraState();
        Step("viewport.zoom", [&] { input.UpdateWheelState(0, 1); });
        Require(CameraState() != panned, "Scroll did not zoom the camera");
        Step("sidebar.pointer", [&] { input.UpdatePointerState(180 * dpi, 450 * dpi); });
        const auto zoomed = CameraState();
        Step("sidebar.scroll", [&] { input.UpdateWheelState(0, -1); });
        Require(CameraState() == zoomed, "Sidebar scrolling changed the camera");
        Click("camera.home");
        Require(CameraState() == before, "Reset did not restore the camera");
    }

    auto NodeControl(std::string_view name) const -> std::string {
        const auto snapshot = m_Viewer->Snapshot();
        const std::string marker = "\"name\":\"" + std::string(name) + "\"";
        for (auto offset = snapshot.find(marker); offset != std::string::npos; offset = snapshot.find(marker, offset + marker.size())) {
            const auto begin = snapshot.rfind("\"id\":\"", offset);
            if (begin == std::string::npos) continue;
            const auto start = begin + 6;
            const auto id = "node:" + snapshot.substr(start, snapshot.find('"', start) - start);
            if (m_Viewer->Controls().contains(id)) return id;
        }
        throw std::runtime_error(std::format("Scene node not found: {}", name));
    }

    void Text(std::u16string value) {
        Step("type", [&] {
            auto& input = m_App->GetInputState();
            Require(input.GetTextInputSession() != 0, "Text field did not acquire focus");
            input.PushTextInput({.kind = hid::TextInputEvent::Kind::Key, .key = 65, .control = true});
            input.PushTextInput({.kind = hid::TextInputEvent::Kind::Commit, .text = value});
            input.PushTextInput({.kind = hid::TextInputEvent::Kind::Key, .key = 13});
        });
    }

    void Contains(std::string_view value) {
        Require(m_Viewer->Snapshot().contains(value), std::format("State does not contain {}", value));
    }

    void RecordScene(unsigned frames, unsigned light_steps = 8, std::string_view physics_mode = {}, std::string_view view = "front") {
        Step("boot");
        Step("layout");
        Contains("\"error\":\"\"");
        Require(view == "front" || view == "right" || view == "portrait", "Recording view must be front, right or portrait");
        Click(view == "right" ? "camera.right" : "camera.front");
        if (view == "portrait") {
            auto& input = m_App->GetInputState();
            const auto viewport = m_Viewer->Controls().at("viewport").rect;
            const float dpi = m_App->GetDpiRatio();
            const float x = (viewport.x + viewport.z * 0.5f) * dpi;
            const float y = (viewport.y + viewport.w * 0.5f) * dpi;
            Step("portrait.pointer", [&] { input.UpdatePointerState(x, y); });
            Step("portrait.pan.down", [&] { input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_M_BUTTON, true); });
            Step("portrait.pan.move", [&] { input.UpdatePointerState(x, y + viewport.w * 0.25f * dpi); });
            Step("portrait.pan.up", [&] { input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_M_BUTTON, false); });
            Step("portrait.zoom", [&] { input.UpdateWheelState(0, 9); });
        }
        Require(light_steps <= 8, "Recording light steps must be in [0, 8]");
        for (unsigned i = 0; i < 8; ++i) {
            if (i < light_steps) {
                Click("light.up");
            } else {
                Step("lighting-setup");
                Step("lighting-setup");
            }
        }
        if (!physics_mode.empty()) {
            if (physics_mode == "physics-off" && m_Viewer->Snapshot().contains("\"physics\":true"))
                Click("physics.toggle");
            else {
                Step("physics-setup");
                Step("physics-setup");
            }
            Click("animation.restart");
        }
        for (unsigned i = 0; i < frames; ++i) {
            Step("scene-playback");
        }
        std::println("Recorded {} GPU frames at 60 fps to {}", m_Viewer->FrameIndex(), m_Output.string());
    }

    void PhysicsJourney(bool gpu_palette = false) {
        m_Viewer->SetGpuPalette(gpu_palette);
        Step("boot");
        Step("layout");
        Contains("\"error\":\"\"");
        Contains("\"physics\":true");
        Contains("\"physicsBackend\":\"Jolt\"");
        Click("camera.front");
        for (unsigned i = 0; i < 30; ++i) Step("simulate");
        Click("animation.toggle");
        Step("paused");
        const auto paused = m_ViewportPixels;
        Step("still-paused");
        Require(paused == m_ViewportPixels, "Paused physics changed pixels");
        Click("physics.toggle");
        Contains("\"physics\":false");
        Require(paused != m_ViewportPixels, "Disabling physics did not change the deformed pose");
        Click("physics.toggle");
        Contains("\"physics\":true");
        Click("animation.restart");
        Step("reset-paused");
        const auto reset = m_ViewportPixels;
        Click("animation.toggle");
        for (unsigned i = 0; i < 30; ++i) Step("simulate-again");
        Click("animation.toggle");
        Click("animation.restart");
        Step("reset-again");
        Require(reset == m_ViewportPixels, "Restart did not reproduce the initial physics pose");
    }

    void Benchmark(unsigned frames, std::string_view physics_mode = "jolt", bool cpu_skinning = false, bool all_visible = false, bool gpu_palette = false) {
        m_Viewer->SetVisibilityQueryBypass(all_visible);
        m_Viewer->SetCpuSkinningReference(cpu_skinning);
        m_Viewer->SetGpuPalette(gpu_palette);
        if (physics_mode == "physics-off") {
            Step("boot");
            Step("layout");
            if (m_Viewer->Snapshot().contains("\"physics\":true")) Click("physics.toggle");
        }
        // Use the normal frames-in-flight path. Drain before/after the measured
        // batch so throughput includes GPU completion, without forcing a stall
        // after every frame. No screenshot/readback/PNG encoding.
        auto tick = [&] {
            // Match the interactive loop: consume native resize events and clear
            // per-poll flags instead of replaying a stale resize every frame.
            m_App->PollEvents();
            m_Host.TickFrame();
        };
        for (unsigned i = 0; i < 60; ++i) {
            tick();
        }
        auto milliseconds = std::make_unique<double[]>(frames);
        std::array<double, 4> phases{};
        m_Graphics->queues.WaitIdle();
        const auto batch_start = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < frames; ++i) {
            ZoneScopedNS("Benchmark.MeasuredFrame", 0);
            FrameMark;
            const auto start = std::chrono::steady_clock::now();
            tick();
            for (unsigned j = 0; j < 4; ++j) phases[j] += m_Viewer->FrameTimings()[j] / frames;
            milliseconds[i] = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - start)
                                          .count();
        }
        m_Graphics->queues.WaitIdle();
        const double batch_ms = std::chrono::duration<double, std::milli>(
                                    std::chrono::steady_clock::now() - batch_start)
                                    .count();
        std::sort(milliseconds.get(), milliseconds.get() + frames);
        std::ofstream report(m_Output / "benchmark.json");
        report << std::format(R"({{"frames":{},"width":{},"height":{},"gpu_completed_batch_ms":{},"throughput_fps":{},"cpu_frame_ms":{{"p50":{},"p95":{},"max":{}}},"mean_phase_ms":{{"gui":{},"scene":{},"render":{},"publish":{}}},"state":{}}})",
                              frames, m_App->GetWindowWidth(), m_App->GetWindowHeight(), batch_ms, frames * 1000.0 / batch_ms,
                              milliseconds[frames / 2], milliseconds[frames * 95 / 100], milliseconds[frames - 1],
                              phases[0], phases[1], phases[2], phases[3],
                              m_Viewer->Snapshot());
        std::println("GPU-completed throughput {} fps; CPU frame p50 {} ms, p95 {} ms",
                     frames * 1000.0 / batch_ms, milliseconds[frames / 2], milliseconds[frames * 95 / 100]);
        std::println("Mean phases: GUI {} scene {} render {} publish {} ms", phases[0], phases[1], phases[2], phases[3]);
    }

    void RecordAnimation() {
        auto hold = [&](unsigned frames, std::string_view action) {
            for (unsigned i = 0; i < frames; ++i) Step(action);
        };
        Step("boot");
        Step("layout");
        Click("camera.front");
        Click("collapse:" + NodeControl("Left").substr(5));
        Click("collapse:" + NodeControl("Right").substr(5));
        Click("animation.toggle");
        Click("animation.restart");
        hold(60, "initial-pose");
        Click("animation.toggle");
        hold(300, "pbr-playback");
        Click("animation.toggle");
        hold(120, "paused");
        Click("animation.restart");
        hold(60, "restarted");
        Click("animation.toggle");
        hold(180, "resumed");
        Click("renderer.deferred");
        hold(300, "deferred-playback");
        Click("animation.toggle");
        hold(60, "final-pose");
        std::println("Recorded {} GPU frames at 60 fps to {}", m_Viewer->FrameIndex(), m_Output.string());
    }

    void Journey() {
        Step("boot");
        Step("layout");
        Contains("\"meshes\":4");
        Contains("\"error\":\"\"");
        CameraJourney();
        const auto coral = NodeControl("Coral");
        Click(coral);
        Step("selection-visible");
        Contains("\"selected\":\"" + coral.substr(5) + "\"");
        const auto shown_pixels = m_ViewportPixels;
        Click("selection.visibility");
        Contains("\"visible\":3");
        Require(m_ViewportPixels != shown_pixels, "Hiding a mesh did not change the rendered viewport");
        Step("hidden-visible");
        Click("selection.visibility");
        Contains("\"visible\":4");
        Require(m_ViewportPixels == shown_pixels, "Showing the mesh did not restore the rendered viewport");
        // Parent visibility must propagate through ECS hierarchy, without altering
        // a child's own hidden override.
        const auto parent_path = NodeControl("Studio").substr(5);
        Click("node:" + parent_path);
        Click("selection.visibility");
        Contains("\"visible\":0");
        Require(m_ViewportPixels != shown_pixels, "Hiding parent did not hide child meshes");
        Click("selection.visibility");
        Contains("\"visible\":4");
        Require(m_ViewportPixels == shown_pixels, "Showing parent did not restore child meshes");
        Click(coral);
        Click("selection.visibility"); // Keep Coral hidden through a parent hide/show.
        Contains("\"visible\":3");
        Click("node:" + parent_path);
        Click("selection.visibility");
        Contains("\"visible\":0");
        Click("selection.visibility");
        Contains("\"visible\":3");
        Click("tree.show-all");
        Contains("\"visible\":4");
        Require(m_ViewportPixels == shown_pixels, "Show all did not clear descendant overrides");
        Click(coral);
        const auto parent = "collapse:" + parent_path;
        Click(parent);
        Require(!m_Viewer->Controls().contains(coral), "Collapsed children remained interactive");
        Click(parent);
        Require(m_Viewer->Controls().contains(coral), "Expanding did not restore children");
        Click("camera.fit");
        Click("camera.front");
        Click("camera.home");
        Click("renderer.deferred");
        Contains("\"renderer\":\"deferred\"");
        Step("deferred-controls");
        Click("debug.normal");
        Contains("\"debug\":2");
        Click("debug.roughness");
        Contains("\"debug\":4");
        Click("debug.final");
        Click("renderer.pbr");
        Contains("\"renderer\":\"pbr\"");
        Step("pbr-controls");
        Require(!m_Viewer->Controls().at("debug.normal").enabled, "PBR must disable deferred views");
        Click("debug.normal", true);
        Contains("\"debug\":0");
        Click("tree.search");
        Text(u"Jade");
        Step("search-results");
        Require(!m_Viewer->Controls().contains(coral), "Search left a filtered node active");
        Click(NodeControl("Jade"));
        Step("jade-selection");
        Contains("\"name\":\"Jade\"");
        Click("file.open");
        Click("file.path");
        Text(u"temp/does-not-exist.usda");
        Click("file.confirm");
        Contains("Scene does not exist");
        Contains("\"meshes\":4");
        Step("error-visible");
        Click("file.confirm");
        Step("same-error-visible");
        Require(m_Viewer->Controls().contains("file.path"),
                "Repeating an invalid path closed its error panel");
        Click("sample.texture");
        Contains("pbr_texture_golden.usda");
        Contains("\"error\":\"\"");
        Step("texture-visible");
        Click("file.reload");
        Contains("\"meshes\":1");
        Step("reload-visible");
        Click("file.open");
        Click("sample.skinned");
        Contains("\"animations\":2");
        Contains("\"playing\":true");
        Step("animation-layout");
        Click("camera.front");
        const auto initial_pose = m_ViewportPixels;
        Step("animation-advance");
        Require(m_ViewportPixels != initial_pose, "Skeletal playback did not change the viewport");
        Click("animation.toggle");
        Contains("\"playing\":false");
        Step("animation-paused");
        const auto paused_pose = m_ViewportPixels;
        Step("animation-still-paused");
        Require(m_ViewportPixels == paused_pose, "Paused animation continued to move");
        Click("renderer.toon");
        Contains("\"renderer\":\"toon\"");
        Step("animation-toon-paused");
        const auto toon_paused_pose = m_ViewportPixels;
        Step("animation-toon-still-paused");
        Require(m_ViewportPixels == toon_paused_pose, "Switching to Toon resumed paused animation");
        Click("animation.toggle");
        Contains("\"playing\":true");
        const auto toon_pose = m_ViewportPixels;
        Step("animation-toon-advance");
        Require(m_ViewportPixels != toon_pose, "Toon skeletal playback did not change the viewport");
        Click("animation.toggle");
        Click("animation.restart");
        Contains("\"animationTime\":0");
        Require(m_ViewportPixels != paused_pose, "Restart did not restore the initial pose");
        Click("renderer.deferred");
        Step("animation-deferred-paused");
        Click("animation.toggle");
        Contains("\"playing\":true");
        const auto deferred_pose = m_ViewportPixels;
        Step("animation-deferred-advance");
        Require(m_ViewportPixels != deferred_pose, "Deferred skeletal playback did not change the viewport");
        Click("renderer.pbr");
        Click("file.open");
        Click("sample.studio");
        Contains("\"meshes\":4");
        Step("studio-restored");
        Click("tree.show-all");
        Click(NodeControl("Coral"));
        Step("inspector-visible");
        Require(m_Viewer->Controls().contains("selection.frame"), "Selection did not show inspector controls");
        if (!m_FixedSize) {
            Step("resize.compact", [&] { m_App->ResizeWindow(800 * m_App->GetDpiRatio(), 600 * m_App->GetDpiRatio()); });
            Require(!m_Viewer->Controls().contains("selection.frame"),
                    "Compact layout kept hidden inspector controls");
            Click("renderer.deferred");
            Step("resize.wide", [&] { m_App->ResizeWindow(1000 * m_App->GetDpiRatio(), 700 * m_App->GetDpiRatio()); });
            Require(m_Viewer->Controls().contains("selection.frame"),
                    "Wide layout did not restore inspector controls");
        }
        Click("renderer.pbr");
        Step("final");
#ifdef _WIN32
        Step("window.close",
             [&] { SendMessageW(static_cast<HWND>(m_App->GetWindow().ptr), WM_CLOSE, 0, 0); });
        Require(m_App->IsQuit(), "Window close did not request application shutdown");
#endif
        Require(!m_Replay || m_Viewer->FrameIndex() == m_Tape.size(), "Replay ended early");
    }

private:
    std::filesystem::path                   m_Output;
    bool                                    m_Replay;
    bool                                    m_RecordAnimation;
    bool                                    m_FixedSize;
    std::vector<RecordedFrame>&             m_Tape;
    std::vector<std::byte>                  m_ViewportPixels;
    core::Runtime&                          m_Runtime;
    std::unique_ptr<platform::PlatformHost> m_App;
    App                                     m_Host{m_Runtime, "ViewerE2E"};
    gfx::GraphicsContext*                   m_Graphics = nullptr;
    USDViewer*                              m_Viewer   = nullptr;
    std::ofstream                           m_Trace;
};

}  // namespace hitagi::examples::test

int main(int argc, char** argv) {
    using namespace hitagi;
    try {
        core::Runtime     runtime;
        const std::string backend = argc > 1 ? argv[1] : "Vulkan";
        examples::test::Require(backend == "Vulkan" || backend == "DX12" || backend == "Metal", "Backend must be Vulkan, DX12 or Metal");
        const auto        type         = backend == "Vulkan" ? gfx::Device::Type::Vulkan : backend == "Metal" ? gfx::Device::Type::Metal
                                                                                                              : gfx::Device::Type::DX12;
        const bool        fixed_size   = argc > 2 && std::string_view(argv[2]) == "--fixed-size";
        const bool        record       = argc > 2 && std::string_view(argv[2]) == "--record-animation";
        const bool        physics_test = argc > 2 && std::string_view(argv[2]) == "--verify-physics";
        const bool        benchmark    = argc > 2 && std::string_view(argv[2]) == "--benchmark";
        const auto        record_scene = (record || physics_test || benchmark) && argc > 3 ? std::filesystem::path(argv[3]) : std::filesystem::path{};
        const bool        toon         = benchmark || (record && argc > 6 && std::string_view(argv[6]) == "toon");
        const std::string physics_mode = record && argc > 7 ? argv[7] : benchmark ? (argc > 5 ? argv[5] : "jolt") : physics_test ? "jolt" : "";
        examples::test::Require(physics_mode.empty() || physics_mode == "physics-off" || physics_mode == "physics-on" || physics_mode == "jolt",
                                "Physics recording mode must be physics-off, physics-on or jolt");
        if (physics_test && argc > 4)
            examples::test::Require(std::string_view(argv[4]) == "jolt", "Only Jolt physics is supported");
        const bool cpu_skinning = benchmark && argc > 6 && std::string_view(argv[6]) == "cpu";
        const auto has_option = [&](std::string_view option) {
            for (int i = 2; i < argc; ++i) if (std::string_view(argv[i]) == option) return true;
            return false;
        };
        const bool all_visible = benchmark && has_option("--all-visible");
        const bool fixed_step = benchmark && has_option("--fixed-step");
        const auto output = std::filesystem::path(record ? "temp/usd-viewer-recording" : physics_test ? "temp/usd-viewer-physics-e2e"
                                                                                                      : "temp/usd-viewer-e2e") /
                            record_scene.stem() /
                            ((toon ? backend + "-toon" : backend) + (physics_mode.empty() ? "" : "-" + physics_mode) + (cpu_skinning ? "-cpu-skinning" : "") + (all_visible ? "-all-visible" : "") + (fixed_step ? "-fixed-step" : ""));
        std::filesystem::create_directories(output);
        std::vector<examples::test::RecordedFrame> tape;
        if (benchmark) {
            examples::test::Session session(runtime, type, output, false, tape, true, record_scene,
                                            examples::RendererType::Toon, fixed_step, false, false);
            const unsigned          frames = argc > 4 ? std::stoul(argv[4]) : 300;
            examples::test::Require(frames > 0, "Benchmark requires frames");
            session.Benchmark(frames, physics_mode, cpu_skinning, all_visible, has_option("--gpu-palette"));
            return 0;
        }
        if (physics_test) {
            examples::test::Require(!record_scene.empty(), "--verify-physics requires a scene");
            for (bool replay : {false, true}) {
                examples::test::Session session(runtime, type, output, replay, tape, false, record_scene,
                                                examples::RendererType::Toon);
                session.PhysicsJourney(has_option("--gpu-palette"));
            }
            std::println("PASS: {} physics ticks replayed with identical state and PNG bytes", tape.size());
            return 0;
        }
        if (record) {
            examples::test::Session session(runtime, type, output, false, tape, true, record_scene,
                                            toon ? examples::RendererType::Toon : examples::RendererType::Pbr);
            if (record_scene.empty())
                session.RecordAnimation();
            else
                session.RecordScene(argc > 4 ? std::stoul(argv[4]) : 1200,
                                    argc > 5 ? std::stoul(argv[5]) : 8, physics_mode, argc > 8 ? argv[8] : "front");
            return 0;
        }
        {
            examples::test::Session session(runtime, type, output, false, tape, false, {}, examples::RendererType::Pbr, true, fixed_size);
            session.Journey();
        }
        {
            examples::test::Session session(runtime, type, output, true, tape, false, {}, examples::RendererType::Pbr, true, fixed_size);
            session.Journey();
        }
        std::println("PASS: {} UI ticks replayed with identical state and PNG bytes ({})", tape.size(),
                     backend);
    } catch (const std::exception& error) {
        std::cerr << "USD viewer E2E failed: " << error.what() << '\n';
        return 1;
    }
}
