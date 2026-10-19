export module gui:manager;
import interop.pocketjs;

import std;
import core;
import gfx;
import app;
import math;
import hid;
import utils;
import :draw_data;
import :pocket_draw;

template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::interop::pocketjs::Button> {
    static constexpr bool is_flags = true;
};

export namespace hitagi::gui {

struct GuiAppOptions {
    bool          desktop        = false;  // DPI-aware window layout and the overlay desktop-pointer channel.
    std::uint32_t raster_density = 1;      // Must match the bundle's font/resource density.
    bool single_step = false;             // Exactly one virtual frame per PrepareFrame/Tick pair.
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
    auto DrainEffects() -> std::pmr::vector<std::pmr::string>;
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
    core::FileIOManager&   m_FileIO;
    GuiDrawData            m_DrawData;
};
}  // namespace hitagi::gui

namespace hitagi::gui {
using Button  = interop::pocketjs::Button;
using Runtime = std::unique_ptr<PocketApple, decltype(&pocket_apple_destroy)>;

constexpr std::uint32_t logical_width = 960, logical_height = 540;
constexpr double        fixed_step = 1.0 / 60.0;

struct GuiManager::State {
    struct Frame {
        core::Clock clock;
        double      accumulator = fixed_step;
        bool        prepared    = false;
    };

    struct TextInput {
        std::uint64_t session = 0;
        math::vec4f   rect{};
        math::vec2f   caret{};
        bool          active_this_frame = false;
    };

    struct Keyboard {
        bool   navigation_enabled   = true;
        Button pending_buttons      = Button::None;
        Button text_blocked_buttons = Button::None;
    };

    struct Pointer {
        math::vec2f                  position{-1, -1};
        bool                         down = false;
        std::optional<std::uint32_t> pending_touch;
    };

    struct Rendering {
        PocketDrawDecoder                                           decoder;
        GuiDrawList                                                 geometry;
        std::pmr::vector<std::pair<rg::TextureHandle, math::vec4f>> images;
    };

    struct Effects {
        std::pmr::vector<std::pmr::string> messages;
        bool                               overflow = false;
    };

    GuiAppOptions options;
    math::vec2u   viewport{logical_width, logical_height};
    Frame         frame;
    TextInput     text_input;
    Keyboard      keyboard;
    Pointer       pointer;
    Rendering     rendering;
    Effects       effects;
    Runtime       runtime{nullptr, pocket_apple_destroy};

    static void ReceiveEffect(const char* line, void* context) noexcept {
        auto& state = *static_cast<State*>(context);
        try {
            if (state.effects.messages.size() < 256)
                state.effects.messages.emplace_back(line);
            else
                state.effects.overflow = true;
        } catch (...) {
            state.effects.overflow = true;
        }
    }
};

GuiManager::GuiManager(Application& application, core::FileIOManager& file_io)
    : RuntimeModule("GuiManager"), m_State(std::make_unique<State>()), m_App(application), m_FileIO(file_io) {}

GuiManager::~GuiManager() { m_App.SetTextInputSession(0); }

auto GuiManager::WantsTextInput() const noexcept -> bool {
    return m_State->text_input.session != 0 || m_State->text_input.active_this_frame;
}

void GuiManager::ApplyTextInputEffects() {
    // Private numeric overlay effect, deliberately independent of editor JSON.
    // session + field rectangle + caret position, all in logical coordinates.
    std::erase_if(m_State->effects.messages, [&](const std::pmr::string& line) {
        constexpr std::string_view prefix = "hitagi.text ";
        if (!line.starts_with(prefix)) return false;
        std::istringstream values(line.substr(prefix.size()));
        std::uint64_t      session;
        math::vec4f        rect;
        math::vec2f        caret;
        if (!(values >> session >> rect.x >> rect.y >> rect.z >> rect.w >> caret.x >> caret.y))
            throw std::invalid_argument("Invalid text input effect");
        for (float value : {rect.x, rect.y, rect.z, rect.w, caret.x, caret.y})
            if (!std::isfinite(value) || std::abs(value) > 16384) throw std::invalid_argument("Invalid text input coordinates");
        if (rect.z < 0 || rect.w < 0) throw std::invalid_argument("Invalid text field size");
        m_App.SetTextInputSession(session);
        m_State->text_input.session = session;
        m_State->text_input.rect    = rect;
        m_State->text_input.caret   = caret;
        m_State->text_input.active_this_frame |= session != 0;
        return true;
    });
    if (m_State->text_input.session) {
        const auto caret = DisplayRect({m_State->text_input.caret.x, m_State->text_input.caret.y, 0, 0});
        m_App.SetInputScreenPosition({static_cast<unsigned>(std::max(0.0f, caret.x)), static_cast<unsigned>(std::max(0.0f, caret.y))});
    }
}

void GuiManager::SetKeyboardNavigationEnabled(bool enabled) {
    m_State->keyboard.navigation_enabled = enabled;
    if (!enabled) m_State->keyboard.pending_buttons = Button::None;
}

void GuiManager::LoadApp(const std::filesystem::path& javascript, const std::filesystem::path& pak, GuiAppOptions options) {
    if (options.raster_density < 1 || options.raster_density > 4)
        throw std::invalid_argument("PocketJS density must be 1..4");

    const float dpi    = std::max(m_App.GetDpiRatio(), 1.0f);
    const auto  width  = options.desktop ? std::max(1u, static_cast<std::uint32_t>(std::round(m_App.GetWindowWidth() / dpi))) : logical_width;
    const auto  height = options.desktop ? std::max(1u, static_cast<std::uint32_t>(std::round(m_App.GetWindowHeight() / dpi))) : logical_height;
    if (width > 8192 || height > 8192)
        throw std::invalid_argument("PocketJS viewport exceeds dimension limit");

    const auto& source_buffer = m_FileIO.SyncOpenAndReadBinary(javascript);
    if (source_buffer.Empty())
        throw std::runtime_error(std::format("Empty or unreadable PocketJS bundle: {}", javascript.string()));

    const auto& assets_buffer = m_FileIO.SyncOpenAndReadBinary(pak);
    if (assets_buffer.Empty())
        throw std::runtime_error(std::format("Empty or unreadable PocketJS pack: {}", pak.string()));

    const auto source = source_buffer.Span<std::uint8_t>();
    const auto assets = assets_buffer.Span<std::uint8_t>();

    auto candidate      = std::make_unique<State>();
    candidate->options  = options;
    candidate->viewport = {width, height};
    candidate->runtime.reset(pocket_create_gpu(options.raster_density, width, height));
    if (!candidate->runtime)
        throw std::runtime_error(std::format("PocketJS create failed: {}", pocket_apple_last_error()));

    const auto runtime = candidate->runtime.get();
    if (pocket_apple_set_identity(runtime, "hitagi", 1) != 0 ||
        pocket_apple_set_svc_allowlist(runtime, "pocket.overlay") != 0 ||
        pocket_apple_set_effect_callback(runtime, State::ReceiveEffect, candidate.get()) != 0 ||
        pocket_apple_load_pak(runtime, assets.data(), assets.size()) != 0 ||
        pocket_apple_eval_bundle(runtime, source.data(), source.size(), "hitagi-ui") != 0) {
        throw std::runtime_error(std::format("PocketJS set failed: {}", pocket_apple_last_error()));
    }

    m_App.SetTextInputSession(0);
    m_App.GetInputManager().DrainTextInput();
    candidate->frame.clock.Start();
    m_State    = std::move(candidate);
    m_DrawData = {};
}

void GuiManager::PostEvent(std::string_view event) {
    if (!m_State->runtime) throw std::logic_error("Load a PocketJS app before posting events");
    const std::pmr::string line(event);
    if (pocket_apple_post_event(m_State->runtime.get(), line.c_str()) != 0)
        throw std::runtime_error(std::format("PocketJS post event failed: {}", pocket_apple_last_error()));
}

auto GuiManager::DrainEffects() -> std::pmr::vector<std::pmr::string> {
    if (std::exchange(m_State->effects.overflow, false))
        throw std::runtime_error("PocketJS effect queue overflow");
    return std::exchange(m_State->effects.messages, {});
}

void GuiManager::DrawTexture(rg::TextureHandle texture, math::vec4f rect) {
    m_State->rendering.images.emplace_back(texture, rect);
}

auto GuiManager::DisplayRect(math::vec4f rect) const -> math::vec4f {
    const auto window = m_App.GetWindowRect();
    const auto width  = static_cast<float>(window.right - window.left);
    const auto height = static_cast<float>(window.bottom - window.top);
    if (m_State->options.desktop) {
        const float dpi = std::max(m_App.GetDpiRatio(), 1.0f);
        return {rect.x * dpi, rect.y * dpi, rect.z * dpi, rect.w * dpi};
    }
    const auto scale = std::min(width / m_State->viewport.x, height / m_State->viewport.y);
    return {rect.x * scale + (width - m_State->viewport.x * scale) * 0.5f,
            rect.y * scale + (height - m_State->viewport.y * scale) * 0.5f,
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
    if (!m_State->runtime || m_State->frame.prepared) return;
    m_State->frame.prepared = true;
    if (m_State->options.single_step) {
        m_State->frame.accumulator = fixed_step;
    } else {
        m_State->frame.clock.Tick();
        m_State->frame.accumulator += std::min(m_State->frame.clock.DeltaTime().count(), 0.1);
    }

    const auto rect   = m_App.GetWindowRect();
    const auto width  = static_cast<float>(rect.right - rect.left);
    const auto height = static_cast<float>(rect.bottom - rect.top);

    if (width <= 0 || height <= 0) {
        if (m_State->options.desktop && m_State->pointer.down) PostEvent(R"({"type":"pointer","event":{"kind":"cancel"}})");
        m_State->pointer.down = false;
        m_DrawData.draw_lists.clear();
        return;
    }

    if (m_State->options.desktop) {
        const float dpi         = std::max(m_App.GetDpiRatio(), 1.0f);
        const auto  next_width  = std::max(1u, static_cast<std::uint32_t>(std::round(width / dpi)));
        const auto  next_height = std::max(1u, static_cast<std::uint32_t>(std::round(height / dpi)));
        if (m_State->viewport.x != next_width || m_State->viewport.y != next_height) {
            if (pocket_resize(m_State->runtime.get(), next_width, next_height) != 0)
                throw std::runtime_error(std::format("PocketJS resize failed: {}", pocket_apple_last_error()));

            m_State->viewport.x         = next_width;
            m_State->viewport.y         = next_height;
            m_State->rendering.geometry = {};
            m_State->frame.accumulator  = std::max(m_State->frame.accumulator, fixed_step);
            PostEvent(R"({"type":"pointer","event":{"kind":"cancel"}})");
            // A held button must not start a new press against the resized tree.
            m_State->pointer.down = m_App.GetInputManager().GetBool(hid::VirtualKeyCode::MOUSE_L_BUTTON);
            PostEvent(std::format(R"({{"type":"state","value":{{"kind":"viewport","width":{},"height":{}}}}})", next_width, next_height));
        }
    }

    const auto  bounds = DisplayRect({0, 0, static_cast<float>(m_State->viewport.x), static_cast<float>(m_State->viewport.y)});
    const float left = bounds.x, top = bounds.y;
    const float scale_x = bounds.z / m_State->viewport.x, scale_y = bounds.w / m_State->viewport.y;

    auto& input                           = m_App.GetInputManager();
    m_State->text_input.active_this_frame = m_State->text_input.session != 0;
    for (const auto& event : input.DrainTextInput()) {
        if (!event.session || event.session != m_State->text_input.session) continue;
        // Escape every UTF-16 code unit, including quotes/control characters.
        std::pmr::string text;
        for (const auto unit : event.text)
            text += std::format("\\u{:04x}", static_cast<unsigned>(unit));

        const auto array = [](const auto& values) {
            std::pmr::string result;
            for (const auto value : values) {
                if (!result.empty()) result += ',';
                result += std::format("{}", value);
            }
            return result;
        };

        constexpr auto kind_names = std::array{"start", "update", "commit", "end", "key", "blur"};
        const auto     kind       = kind_names.at(static_cast<std::size_t>(event.kind));

        PostEvent(std::format(R"({{"type":"state","value":{{"kind":"text","session":{},"event":"{}","text":"{}","caret":{},"attributes":[{}],"clauses":[{}],"key":{},"shift":{},"control":{}}}}})",
                              event.session, kind_names.at(static_cast<std::size_t>(event.kind)), text, event.caret, array(event.attributes), array(event.clauses), event.key, event.shift, event.control));
        m_State->frame.accumulator = std::max(m_State->frame.accumulator, fixed_step);
        if (event.kind == hid::TextInputEvent::Kind::Blur) {
            m_App.SetTextInputSession(0);
            m_State->text_input.session = 0;
        }
    }
    using Key                   = hid::VirtualKeyCode;
    Button         buttons      = Button::None;
    Button         held_buttons = Button::None;
    constexpr auto key_masks    = std::array{
        std::pair{Key::KEY_UP, Button::Up},
        std::pair{Key::KEY_RIGHT, Button::Right},
        std::pair{Key::KEY_DOWN, Button::Down},
        std::pair{Key::KEY_LEFT, Button::Left},
        std::pair{Key::KEY_ENTER, Button::Circle},
        std::pair{Key::KEY_SPACE, Button::Circle},
        std::pair{Key::KEY_ESCAPE, Button::Cross},
    };
    // PocketJS's portable focus controller activates with Circle and cancels with Cross.
    for (const auto [key, mask] : key_masks) {
        if (input.GetBool(key)) held_buttons |= mask;
        if (m_State->keyboard.navigation_enabled && !WantsTextInput() && input.GetBool(key)) buttons |= mask;
        if (m_State->keyboard.navigation_enabled && !WantsTextInput() && input.GetBoolNew(key)) m_State->keyboard.pending_buttons |= mask;
    }
    if (WantsTextInput()) {
        m_State->keyboard.text_blocked_buttons |= held_buttons;
    }
    m_State->keyboard.text_blocked_buttons &= held_buttons;
    buttons &= ~m_State->keyboard.text_blocked_buttons;
    m_State->keyboard.pending_buttons &= ~m_State->keyboard.text_blocked_buttons;

    const float x = (input.GetFloat(hid::MouseEvent::MOVE_X) - left) / scale_x;
    const float y = (input.GetFloat(hid::MouseEvent::MOVE_Y) - top) / scale_y;

    if (m_State->options.desktop) {
        if (m_State->pointer.position.x != x || m_State->pointer.position.y != y) {
            PostEvent(std::format(R"({{"type":"pointer","event":{{"kind":"move","x":{},"y":{}}}}})", x, y));
            m_State->pointer.position = {x, y};
        }
        const bool down = input.GetBool(Key::MOUSE_L_BUTTON);
        if (down && !m_State->pointer.down && m_State->text_input.session && !math::contains(m_State->text_input.rect, {x, y})) {
            PostEvent(std::format(R"({{"type":"state","value":{{"kind":"text","session":{},"event":"blur"}}}})", m_State->text_input.session));
            m_App.SetTextInputSession(0);
            m_State->text_input.session = 0;
            m_State->frame.accumulator  = std::max(m_State->frame.accumulator, fixed_step);
        }
        if (down != m_State->pointer.down) {
            // Focus loss resets both current and previous key states. Cancel it;
            // treating that reset as a release could accidentally activate a button.
            if (!down && !input.GetBoolReleased(Key::MOUSE_L_BUTTON))
                PostEvent(R"({"type":"pointer","event":{"kind":"cancel"}})");
            else
                PostEvent(std::format(R"({{"type":"pointer","event":{{"kind":"button","x":{},"y":{},"down":{}}}}})", x, y, down));
            m_State->pointer.down = down;
        }
        const auto wheel = input.GetFloatDelta(hid::MouseEvent::SCROLL_Y);
        if (wheel != 0)
            PostEvent(std::format(R"({{"type":"state","value":{{"kind":"wheel","x":{},"y":{},"delta":{}}}}})", x, y, wheel));
    }

    std::optional<std::uint32_t> touch;
    if (!m_State->options.desktop && input.GetBool(Key::MOUSE_L_BUTTON) && x >= 0 && x < m_State->viewport.x && y >= 0 && y < m_State->viewport.y) {
        // Wide PocketJS contact format: bit 31, id:8, y:10, x:10.
        touch = 0x80000000u | (static_cast<std::uint32_t>(y) << 10) | static_cast<std::uint32_t>(x);
        if (input.GetBoolNew(Key::MOUSE_L_BUTTON)) m_State->pointer.pending_touch = touch;
    }

    bool advanced = false;
    if (WantsTextInput()) m_State->keyboard.pending_buttons = Button::None;
    while (m_State->frame.accumulator >= fixed_step) {
        const auto contact = m_State->pointer.pending_touch ? m_State->pointer.pending_touch : touch;
        if (pocket_apple_frame(
                m_State->runtime.get(),
                std::to_underlying(buttons | m_State->keyboard.pending_buttons),
                0,
                contact ? &*contact : nullptr,
                contact ? 1 : 0) != 0)
            throw std::runtime_error(std::format("PocketJS frame failed: {}", pocket_apple_last_error()));

        ApplyTextInputEffects();
        if (WantsTextInput())
            buttons = Button::None;

        m_State->keyboard.pending_buttons = Button::None;
        m_State->pointer.pending_touch.reset();
        m_State->frame.accumulator -= fixed_step;
        advanced = true;
    }

    if (advanced || m_State->rendering.geometry.vertices.empty()) {
        m_State->rendering.geometry = m_State->rendering.decoder.Build(m_State->runtime.get());
        const auto resources        = m_State->rendering.decoder.Textures();
        m_DrawData.textures.assign(resources.begin(), resources.end());
    }
    ApplyTextInputEffects();
}

void GuiManager::Tick() {
    PrepareFrame();
    m_State->frame.prepared = false;
    const auto images       = std::exchange(m_State->rendering.images, {});
    if (!m_State->runtime) return;
    const auto width  = static_cast<float>(m_App.GetWindowWidth());
    const auto height = static_cast<float>(m_App.GetWindowHeight());
    if (width <= 0 || height <= 0) {
        m_DrawData.draw_lists.clear();
        return;
    }
    const auto  bounds = DisplayRect({0, 0, static_cast<float>(m_State->viewport.x), static_cast<float>(m_State->viewport.y)});
    const float left = bounds.x, top = bounds.y;
    m_DrawData.display_pos  = {};
    m_DrawData.display_size = {width, height};
    m_DrawData.draw_lists.clear();
    auto&       list  = m_DrawData.draw_lists.emplace_back(m_State->rendering.geometry);
    const float scale = bounds.z / m_State->viewport.x;

    for (auto& vertex : list.vertices)
        vertex.position = {left + vertex.position.x * scale, top + vertex.position.y * scale};

    for (auto& command : list.commands) {
        auto& clip = command.clip_rect;
        clip       = {
            std::max(left, left + clip.x * scale),
            std::max(top, top + clip.y * scale),
            std::min(left + bounds.z, left + clip.z * scale),
            std::min(top + bounds.w, top + clip.w * scale),
        };
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
        overlay.commands.emplace_back(GuiDrawCommand{
            .element_count = 6,
            .clip_rect     = {rect.x, rect.y, rect.x + rect.z, rect.y + rect.w},
            .texture       = {.type = GuiTextureRef::Type::RenderGraph, .texture = texture},
        });
    }
}
}  // namespace hitagi::gui
