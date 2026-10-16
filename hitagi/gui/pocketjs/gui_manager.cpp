export module gui:manager;
import interop.pocketjs;

import std;
import core;
import gfx;
import app;
import math;
import hid;
import :draw_data;
import :pocket_draw;

export namespace hitagi::gui {

struct GuiAppOptions {
    bool          desktop        = false;  // DPI-aware window layout and the overlay desktop-pointer channel.
    std::uint32_t raster_density = 1;      // Must match the bundle's font/resource density.
};

// All calls stay on the engine thread. PocketJS owns a retained component tree;
// callers load a bundle once rather than submitting immediate-mode draw tasks.
class GuiManager final : public core::RuntimeModule {
public:
    GuiManager(Application& application, core::FileIOManager& file_io);
    ~GuiManager() final;
    void Tick() final;
    // Update layout/input before native viewport rendering; Tick later composites textures.
    void PrepareFrame();
    // The active game viewport can own arrows/space without activating UI controls.
    void SetKeyboardNavigationEnabled(bool enabled);
    auto WantsTextInput() const noexcept -> bool;
    void LoadApp(const std::filesystem::path& javascript, const std::filesystem::path& pak, GuiAppOptions options = {});
    void PostEvent(std::string_view event);
    auto DrainEffects() -> std::vector<std::string>;
    // Rectangles use the same logical coordinates as the PocketJS component tree.
    void DrawTexture(rg::TextureHandle texture, math::vec4f rect);
    auto DisplayRect(math::vec4f rect) const -> math::vec4f;
    auto PointerIn(math::vec4f rect) const -> bool;
    auto GetDrawData() const noexcept -> const GuiDrawData& { return m_DrawData; }

private:
    void ApplyTextInputEffects();
    struct State;
    std::unique_ptr<State> m_State;
    Application&           m_App;
    GuiDrawData            m_DrawData;
};
}  // namespace hitagi::gui

namespace hitagi::gui {
constexpr std::uint32_t logical_width = 960, logical_height = 540;
constexpr double        fixed_step = 1.0 / 60.0;

void CheckPocket(int result) {
    if (result != 0) throw std::runtime_error(std::string("PocketJS: ") + pocket_apple_last_error());
}

auto ReadBytes(const std::filesystem::path& path) -> std::vector<std::uint8_t> {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open PocketJS asset: " + path.string());
    const auto length = file.tellg();
    if (length <= 0) throw std::runtime_error("Empty PocketJS asset: " + path.string());
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot read PocketJS asset: " + path.string());
    return bytes;
}
struct GuiManager::State {
    GuiAppOptions                                                 options;
    bool                                                          prepared = false;
    bool                                                          keyboard_navigation = true;
    bool text_input_frame = false;
    std::uint64_t text_session = 0;
    math::vec4f text_rect{};
    math::vec2f text_caret{};
    std::uint32_t                                                 width = logical_width, height = logical_height;
    PocketDrawDecoder decoder;
    GuiDrawList geometry;
    math::vec2f                                                   pointer      = {-1, -1};
    bool                                                          pointer_down = false;
    std::unique_ptr<PocketApple, decltype(&pocket_apple_destroy)> runtime{nullptr, pocket_apple_destroy};

    std::vector<std::string>                                      effects;
    std::vector<std::pair<rg::TextureHandle, math::vec4f>>        images;
    bool                                                          effect_overflow = false;
    static void                                                   ReceiveEffect(const char* line, void* context) noexcept {
        auto& state = *static_cast<State*>(context);
        try {
            if (state.effects.size() < 256)
                state.effects.emplace_back(line);
            else
                state.effect_overflow = true;
        } catch (...) {
            state.effect_overflow = true;
        }
    }
    std::chrono::steady_clock::time_point last_tick       = std::chrono::steady_clock::now();
    double                                accumulator     = fixed_step;

    std::uint32_t                         pending_buttons = 0;
    std::uint32_t                         text_blocked_buttons = 0;
    std::optional<std::uint32_t>          pending_touch;
};

GuiManager::GuiManager(Application& application, core::FileIOManager&)
    : RuntimeModule("GuiManager/PocketJS"), m_State(std::make_unique<State>()), m_App(application) {}

GuiManager::~GuiManager() { m_App.SetTextInputSession(0); }

auto GuiManager::WantsTextInput() const noexcept -> bool {
    return m_State->text_session != 0 || m_State->text_input_frame;
}

void GuiManager::ApplyTextInputEffects() {
    // Private numeric overlay effect, deliberately independent of editor JSON.
    // session + field rectangle + caret position, all in logical coordinates.
    auto& state = *m_State;
    std::erase_if(state.effects, [&](const std::string& line) {
        constexpr std::string_view prefix = "hitagi.text ";
        if (!line.starts_with(prefix)) return false;
        std::istringstream values(line.substr(prefix.size()));
        std::uint64_t session;
        math::vec4f rect;
        math::vec2f caret;
        if (!(values >> session >> rect.x >> rect.y >> rect.z >> rect.w >> caret.x >> caret.y))
            throw std::invalid_argument("Invalid text input effect");
        for (float value : {rect.x, rect.y, rect.z, rect.w, caret.x, caret.y})
            if (!std::isfinite(value) || std::abs(value) > 16384) throw std::invalid_argument("Invalid text input coordinates");
        if (rect.z < 0 || rect.w < 0) throw std::invalid_argument("Invalid text field size");
        m_App.SetTextInputSession(session);
        state.text_session = session;
        state.text_rect = rect;
        state.text_caret = caret;
        state.text_input_frame |= session != 0;
        return true;
    });
    if (state.text_session) {
        const auto caret = DisplayRect({state.text_caret.x, state.text_caret.y, 0, 0});
        m_App.SetInputScreenPosition({static_cast<unsigned>(std::max(0.0f, caret.x)), static_cast<unsigned>(std::max(0.0f, caret.y))});
    }
}

void GuiManager::SetKeyboardNavigationEnabled(bool enabled) {
    m_State->keyboard_navigation = enabled;
    if (!enabled) m_State->pending_buttons = 0;
}

void GuiManager::LoadApp(const std::filesystem::path& javascript, const std::filesystem::path& pak, GuiAppOptions options) {
    if (options.raster_density < 1 || options.raster_density > 4) throw std::invalid_argument("PocketJS density must be 1..4");
    const float dpi    = std::max(m_App.GetDpiRatio(), 1.0f);
    const auto  width  = options.desktop ? std::max(1u, static_cast<std::uint32_t>(std::round(m_App.GetWindowWidth() / dpi))) : logical_width;
    const auto  height = options.desktop ? std::max(1u, static_cast<std::uint32_t>(std::round(m_App.GetWindowHeight() / dpi))) : logical_height;
    if (width > 8192 || height > 8192)
        throw std::invalid_argument("PocketJS viewport exceeds dimension limit");
    auto                       source = ReadBytes(javascript);
    auto                       assets = ReadBytes(pak);
    State                      candidate;
    decltype(m_State->runtime) runtime(pocket_hitagi_create_gpu(options.raster_density, width, height), pocket_apple_destroy);
    if (!runtime) throw std::runtime_error(std::string("PocketJS create failed: ") + pocket_apple_last_error());
    CheckPocket(pocket_apple_set_identity(runtime.get(), "hitagi", 1));
    CheckPocket(pocket_apple_set_svc_allowlist(runtime.get(), "pocket.overlay"));
    CheckPocket(pocket_apple_set_effect_callback(runtime.get(), State::ReceiveEffect, &candidate));
    CheckPocket(pocket_apple_load_pak(runtime.get(), assets.data(), assets.size()));
    CheckPocket(pocket_apple_eval_bundle(runtime.get(), source.data(), source.size(), "hitagi-ui"));
    CheckPocket(pocket_apple_set_effect_callback(runtime.get(), State::ReceiveEffect, m_State.get()));
    m_App.SetTextInputSession(0);
    m_App.GetInputManager().DrainTextInput();
    m_State->text_session = 0;
    m_State->text_input_frame = false;
    m_State->runtime     = std::move(runtime);
    m_State->options     = options;
    m_State->prepared    = false;
    m_State->width       = width;
    m_State->height      = height;
    m_State->decoder = {};
    m_State->geometry = {};
    m_State->pointer                             = {-1, -1};
    m_State->pointer_down                        = false;

    m_State->effects = std::move(candidate.effects);
    m_State->images.clear();
    m_State->effect_overflow = candidate.effect_overflow;
    m_State->accumulator     = fixed_step;
    m_State->last_tick       = std::chrono::steady_clock::now();
    m_State->pending_buttons = 0;
    m_State->text_blocked_buttons = 0;
    m_State->keyboard_navigation = true;
    m_State->pending_touch.reset();
    m_DrawData = {};
}

void GuiManager::PostEvent(std::string_view event) {
    if (!m_State->runtime) throw std::logic_error("Load a PocketJS app before posting events");
    const std::string line(event);
    CheckPocket(pocket_apple_post_event(m_State->runtime.get(), line.c_str()));
}

auto GuiManager::DrainEffects() -> std::vector<std::string> {
    if (std::exchange(m_State->effect_overflow, false))
        throw std::runtime_error("PocketJS effect queue overflow");
    return std::exchange(m_State->effects, {});
}

void GuiManager::DrawTexture(rg::TextureHandle texture, math::vec4f rect) {
    m_State->images.emplace_back(texture, rect);
}

auto GuiManager::DisplayRect(math::vec4f rect) const -> math::vec4f {
    const auto window = m_App.GetWindowRect();
    const auto width  = static_cast<float>(window.right - window.left);
    const auto height = static_cast<float>(window.bottom - window.top);
    if (m_State->options.desktop) {
        const float dpi = std::max(m_App.GetDpiRatio(), 1.0f);
        return {rect.x * dpi, rect.y * dpi, rect.z * dpi, rect.w * dpi};
    }
    const auto scale = std::min(width / m_State->width, height / m_State->height);
    return {rect.x * scale + (width - m_State->width * scale) * 0.5f,
            rect.y * scale + (height - m_State->height * scale) * 0.5f,
            rect.z * scale, rect.w * scale};
}

auto GuiManager::PointerIn(math::vec4f rect) const -> bool {
    const auto  bounds = DisplayRect(rect);
    const auto& input  = m_App.GetInputManager();
    const auto  x      = input.GetFloat(hid::MouseEvent::MOVE_X);
    const auto  y      = input.GetFloat(hid::MouseEvent::MOVE_Y);
    return x >= bounds.x && x < bounds.x + bounds.z && y >= bounds.y && y < bounds.y + bounds.w;
}

void GuiManager::PrepareFrame() {
    auto& state = *m_State;
    if (!state.runtime || state.prepared) return;
    state.prepared = true;
    const auto now = std::chrono::steady_clock::now();
    state.accumulator += std::min(std::chrono::duration<double>(now - state.last_tick).count(), 0.1);
    state.last_tick = now;

    const auto  rect   = m_App.GetWindowRect();
    const float width  = static_cast<float>(rect.right - rect.left);
    const float height = static_cast<float>(rect.bottom - rect.top);
    if (width <= 0 || height <= 0) {
        if (state.options.desktop && state.pointer_down) PostEvent(R"({"type":"pointer","event":{"kind":"cancel"}})");
        state.pointer_down = false;
        m_DrawData.draw_lists.clear();
        return;
    }
    if (state.options.desktop) {
        const float dpi         = std::max(m_App.GetDpiRatio(), 1.0f);
        const auto  next_width  = std::max(1u, static_cast<std::uint32_t>(std::round(width / dpi)));
        const auto  next_height = std::max(1u, static_cast<std::uint32_t>(std::round(height / dpi)));
        if (state.width != next_width || state.height != next_height) {
            CheckPocket(pocket_hitagi_resize(state.runtime.get(), next_width, next_height));
            state.width  = next_width;
            state.height = next_height;
            state.geometry = {};
            state.accumulator = std::max(state.accumulator, fixed_step);
            PostEvent(R"({"type":"pointer","event":{"kind":"cancel"}})");
            // A held button must not start a new press against the resized tree.
            state.pointer_down = m_App.GetInputManager().GetBool(hid::VirtualKeyCode::MOUSE_L_BUTTON);
            PostEvent(std::format(R"({{"type":"state","value":{{"kind":"viewport","width":{},"height":{}}}}})", next_width, next_height));
        }
    }
    const auto  bounds = DisplayRect({0, 0, static_cast<float>(state.width), static_cast<float>(state.height)});
    const float left = bounds.x, top = bounds.y;
    const float scale_x = bounds.z / state.width, scale_y = bounds.w / state.height;
    auto&       input     = m_App.GetInputManager();
    state.text_input_frame = state.text_session != 0;
    for (const auto& event : input.DrainTextInput()) {
        if (!event.session || event.session != state.text_session) continue;
        // Escape every UTF-16 code unit, including quotes/control characters.
        std::string text;
        for (const auto unit : event.text) text += std::format("\\u{:04x}", static_cast<unsigned>(unit));
        const auto array = [](const auto& values) {
            std::string result;
            for (const auto value : values) { if (!result.empty()) result += ','; result += std::to_string(value); }
            return result;
        };
        const auto kind = std::array{"start", "update", "commit", "end", "key", "blur"}.at(static_cast<std::size_t>(event.kind));
        PostEvent(std::format(R"({{"type":"state","value":{{"kind":"text","session":{},"event":"{}","text":"{}","caret":{},"attributes":[{}],"clauses":[{}],"key":{},"shift":{},"control":{}}}}})",
            event.session, kind, text, event.caret, array(event.attributes), array(event.clauses), event.key, event.shift, event.control));
        state.accumulator = std::max(state.accumulator, fixed_step);
        if (event.kind == hid::TextInputEvent::Kind::Blur) {
            m_App.SetTextInputSession(0);
            state.text_session = 0;
        }
    }
    using Key             = hid::VirtualKeyCode;
    std::uint32_t buttons = 0;
    std::uint32_t held_buttons = 0;
    for (const auto [key, mask] : std::array{
             std::pair{Key::KEY_UP, interop::pocket_button_up}, std::pair{Key::KEY_RIGHT, interop::pocket_button_right},
             std::pair{Key::KEY_DOWN, interop::pocket_button_down}, std::pair{Key::KEY_LEFT, interop::pocket_button_left},
             std::pair{Key::KEY_ENTER, interop::pocket_button_confirm}, std::pair{Key::KEY_SPACE, interop::pocket_button_confirm},
             std::pair{Key::KEY_ESCAPE, interop::pocket_button_cancel}}) {
        if (input.GetBool(key)) held_buttons |= mask;
        if (state.keyboard_navigation && !WantsTextInput() && input.GetBool(key)) buttons |= mask;
        if (state.keyboard_navigation && !WantsTextInput() && input.GetBoolNew(key)) state.pending_buttons |= mask;
    }
    if (WantsTextInput()) state.text_blocked_buttons |= held_buttons;
    state.text_blocked_buttons &= held_buttons;
    buttons &= ~state.text_blocked_buttons;
    state.pending_buttons &= ~state.text_blocked_buttons;
    const float x = (input.GetFloat(hid::MouseEvent::MOVE_X) - left) / scale_x;
    const float y = (input.GetFloat(hid::MouseEvent::MOVE_Y) - top) / scale_y;
    if (state.options.desktop) {
        if (state.pointer.x != x || state.pointer.y != y) {
            PostEvent(std::format(R"({{"type":"pointer","event":{{"kind":"move","x":{},"y":{}}}}})", x, y));
            state.pointer = {x, y};
        }
        const bool down = input.GetBool(Key::MOUSE_L_BUTTON);
        if (down && !state.pointer_down && state.text_session &&
            !(x >= state.text_rect.x && y >= state.text_rect.y && x < state.text_rect.x + state.text_rect.z && y < state.text_rect.y + state.text_rect.w)) {
            PostEvent(std::format(R"({{"type":"state","value":{{"kind":"text","session":{},"event":"blur"}}}})", state.text_session));
            m_App.SetTextInputSession(0);
            state.text_session = 0;
            state.accumulator = std::max(state.accumulator, fixed_step);
        }
        if (down != state.pointer_down) {
            // Focus loss resets both current and previous key states. Cancel it;
            // treating that reset as a release could accidentally activate a button.
            if (!down && !input.GetBoolReleased(Key::MOUSE_L_BUTTON))
                PostEvent(R"({"type":"pointer","event":{"kind":"cancel"}})");
            else
                PostEvent(std::format(R"({{"type":"pointer","event":{{"kind":"button","x":{},"y":{},"down":{}}}}})", x, y, down));
            state.pointer_down = down;
        }
        const auto wheel = input.GetFloatDelta(hid::MouseEvent::SCROLL_Y);
        if (wheel != 0)
            PostEvent(std::format(R"({{"type":"state","value":{{"kind":"wheel","x":{},"y":{},"delta":{}}}}})", x, y, wheel));
    }
    std::optional<std::uint32_t> touch;
    if (!state.options.desktop && input.GetBool(Key::MOUSE_L_BUTTON) && x >= 0 && x < state.width && y >= 0 && y < state.height) {
        // Wide PocketJS contact format: bit 31, id:8, y:10, x:10.
        touch = 0x80000000u | (static_cast<std::uint32_t>(y) << 10) | static_cast<std::uint32_t>(x);
        if (input.GetBoolNew(Key::MOUSE_L_BUTTON)) state.pending_touch = touch;
    }
    bool advanced = false;
    if (WantsTextInput()) state.pending_buttons = 0;
    while (state.accumulator >= fixed_step) {
        const auto contact = state.pending_touch ? state.pending_touch : touch;
        CheckPocket(pocket_apple_frame(state.runtime.get(), buttons | state.pending_buttons, 0,
                                       contact ? &*contact : nullptr, contact ? 1 : 0));
        ApplyTextInputEffects();
        if (WantsTextInput()) buttons = 0;
        state.pending_buttons = 0;
        state.pending_touch.reset();
        state.accumulator -= fixed_step;
        advanced = true;
    }
    if (advanced || state.geometry.vertices.empty()) {
        state.geometry = state.decoder.Build(state.runtime.get());
        const auto resources = state.decoder.Textures();
        m_DrawData.textures.assign(resources.begin(), resources.end());
    }
    ApplyTextInputEffects();
}

void GuiManager::Tick() {
    PrepareFrame();
    auto& state       = *m_State;
    state.prepared    = false;
    const auto images = std::exchange(state.images, {});
    if (!state.runtime) return;
    const float width  = static_cast<float>(m_App.GetWindowWidth());
    const float height = static_cast<float>(m_App.GetWindowHeight());
    if (width <= 0 || height <= 0) {
        m_DrawData.draw_lists.clear();
        return;
    }
    const auto  bounds = DisplayRect({0, 0, static_cast<float>(state.width), static_cast<float>(state.height)});
    const float left = bounds.x, top = bounds.y;
    m_DrawData.display_pos  = {};
    m_DrawData.display_size = {width, height};
    m_DrawData.draw_lists.clear();
    auto& list = m_DrawData.draw_lists.emplace_back(state.geometry);
    const float scale = bounds.z / state.width;
    for (auto& vertex : list.vertices)
        vertex.position = {left + vertex.position.x * scale, top + vertex.position.y * scale};
    for (auto& command : list.commands) {
        auto& clip = command.clip_rect;
        clip = {std::max(left, left + clip.x * scale), std::max(top, top + clip.y * scale),
                std::min(left + bounds.z, left + clip.z * scale), std::min(top + bounds.w, top + clip.w * scale)};
    }
    for (const auto& [texture, logical_rect] : images) {
        const auto rect = DisplayRect(logical_rect);
        if (rect.z <= 0 || rect.w <= 0) continue;
        auto& overlay    = m_DrawData.draw_lists.emplace_back();
        overlay.vertices = {
            {.position = {rect.x, rect.y}, .uv = {0, 0}},
            {.position = {rect.x + rect.z, rect.y}, .uv = {1, 0}},
            {.position = {rect.x + rect.z, rect.y + rect.w}, .uv = {1, 1}},
            {.position = {rect.x, rect.y + rect.w}, .uv = {0, 1}},
        };
        overlay.indices = {0, 1, 2, 0, 2, 3};
        overlay.commands.push_back({.element_count = 6,
                                    .clip_rect     = {rect.x, rect.y, rect.x + rect.z, rect.y + rect.w},
                                    .texture       = {GuiTextureRef::Type::RenderGraph, texture}});
    }
}
}  // namespace hitagi::gui
