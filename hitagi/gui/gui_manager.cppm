module;

#include <imgui.h>
#include <spdlog/logger.h>

export module gui;
import std;
import core;
import gfx;
import hid;
import app;
import math;

export namespace hitagi::gui {

struct GuiTextureRef {
    enum class Type : std::uint8_t {
        Font,
        RenderGraph,
    };

    Type              type    = Type::Font;
    rg::TextureHandle texture = {};
};

struct GuiVertex {
    math::vec2f position = {0.0f, 0.0f};
    math::vec2f uv       = {0.0f, 0.0f};
    math::Color color    = math::Color::White();
};

struct GuiDrawCommand {
    std::uint32_t element_count = 0;
    std::uint32_t index_offset  = 0;
    std::uint32_t vertex_offset = 0;
    math::vec4f   clip_rect     = {};
    GuiTextureRef texture       = {};
};

struct GuiDrawList {
    std::pmr::vector<GuiVertex>      vertices;
    std::pmr::vector<std::uint32_t>  indices;
    std::pmr::vector<GuiDrawCommand> commands;
};

struct GuiFontAtlas {
    std::uint32_t              width      = 0;
    std::uint32_t              height     = 0;
    std::span<const std::byte> pixels     = {};
    std::uint64_t              generation = 0;

    constexpr auto Empty() const noexcept -> bool {
        return width == 0 || height == 0 || pixels.empty();
    }
};

struct GuiDrawData {
    math::vec2f                   display_pos  = {0.0f, 0.0f};
    math::vec2f                   display_size = {0.0f, 0.0f};
    GuiFontAtlas                  font_atlas;
    std::pmr::vector<GuiDrawList> draw_lists;

    auto Empty() const noexcept -> bool {
        return draw_lists.empty();
    }
};

class GuiManager final : public RuntimeModule {
public:
    GuiManager(Application& application);
    ~GuiManager() final;
    void Tick() final;

    template <typename DrawFunc>
    inline void DrawGui(DrawFunc&& draw_func) {
        m_GuiDrawTasks.emplace([func = std::forward<DrawFunc>(draw_func)] { func(); });
    }

    auto         ReadTexture(rg::TextureHandle texture) -> ImTextureID;
    inline auto& GetDrawData() const noexcept { return m_DrawData; }

private:
    void LoadFont();
    void BuildDrawData();
    void MouseEvent();
    void KeysEvent();

    static auto DecodeColor(std::uint32_t color) noexcept -> math::Color;
    static auto DecodeTexture(ImTextureID texture_id) noexcept -> GuiTextureRef;

    Application&       m_App;
    hid::InputManager& m_InputManager;
    core::Clock        m_Clock;

    std::queue<std::function<void()>, std::pmr::deque<std::function<void()>>> m_GuiDrawTasks;

    GuiDrawData                 m_DrawData;
    std::pmr::vector<std::byte> m_FontAtlasPixels;
    std::uint32_t               m_FontAtlasWidth      = 0;
    std::uint32_t               m_FontAtlasHeight     = 0;
    std::uint64_t               m_FontAtlasGeneration = 0;
};

}  // namespace hitagi::gui
