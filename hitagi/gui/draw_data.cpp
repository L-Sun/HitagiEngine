export module gui:draw_data;
import std;
import gfx;
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

}  // namespace hitagi::gui
