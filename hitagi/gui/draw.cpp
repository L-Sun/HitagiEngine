export module gui:pocket_draw;
import interop.pocketjs;
import std;
import core;
import math;
import :draw_data;

export namespace hitagi::gui {
// Converts the pinned PocketJS DrawList contract to engine-owned geometry.
// Resource snapshots survive subsequent guest mutations and deferred graph execution.
class PocketDrawDecoder {
public:
    auto Build(PocketApple* runtime) -> GuiDrawList;
    auto Textures() const -> std::vector<std::shared_ptr<const GuiTextureData>>;

private:
    struct Image {
        std::uint64_t                         revision;
        std::shared_ptr<const GuiTextureData> data;
    };
    struct Font {
        std::uint64_t                         revision;
        std::uint32_t                         cell_w, cell_h, coverage_w, coverage_h, columns, glyphs;
        std::shared_ptr<const GuiTextureData> data;
    };
    std::unordered_map<std::int32_t, Image> m_Images;
    std::unordered_map<std::uint32_t, Font> m_Fonts;
    void                                    SyncResources(PocketApple* runtime, const PocketDrawList& frame);
};
}  // namespace hitagi::gui

namespace hitagi::gui {
namespace pocket                              = interop::pocketjs;
constexpr std::uint32_t max_texture_dimension = 16384;
constexpr std::uint64_t max_texture_pixels    = 64u * 1024u * 1024u;
constexpr std::uint32_t glyph_gutter          = 1;

auto NewGuiTexture(std::uint32_t width, std::uint32_t height, bool linear) -> std::shared_ptr<GuiTextureData> {
    static std::atomic<std::uint64_t> next_id{1};
    if (!width || !height || width > max_texture_dimension || height > max_texture_dimension || width * height > max_texture_pixels)
        throw std::runtime_error("Invalid PocketJS texture dimensions");

    return std::make_shared<GuiTextureData>(GuiTextureData{
        .id     = next_id.fetch_add(1, std::memory_order_relaxed),
        .width  = width,
        .height = height,
        .linear = linear,
        .pixels = core::Buffer(width * height * pocket::rgba_bytes_per_pixel),
    });
}

void PocketDrawDecoder::SyncResources(PocketApple* runtime, const PocketDrawList& frame) {
    std::unordered_set<std::int32_t> live;
    for (std::uint32_t slot = 0; slot < frame.texture_slots; ++slot) {
        PocketTexture view{};
        {
            const int status = pocket_texture(runtime, slot, &view);
            if (status == pocket::resource_vacant) continue;
            if (status != pocket::resource_present) throw std::runtime_error("Cannot read PocketJS texture");
            live.insert(view.handle);
        }

        const auto old = m_Images.find(view.handle);
        if (old != m_Images.end() && old->second.revision == view.revision) continue;

        auto data = NewGuiTexture(view.width, view.height, view.linear != 0);

        const auto count           = static_cast<std::size_t>(view.width) * view.height;
        const auto bytes_per_pixel = pocket::BytesPerPixel(view.format);
        if (!bytes_per_pixel) throw std::runtime_error("Unsupported PocketJS pixel format");

        if (!view.pixels ||
            view.byte_count < count * bytes_per_pixel ||
            (view.format == pocket::PixelFormat::Indexed8 && (!view.palette || view.palette_bytes < pocket::palette_entries * pocket::rgba_bytes_per_pixel)))
            throw std::runtime_error("Truncated PocketJS texture");

        const auto source  = std::span{view.pixels, view.byte_count};
        const auto palette = view.format == pocket::PixelFormat::Indexed8
                                 ? std::span{view.palette, view.palette_bytes}
                                 : std::span<const std::uint8_t>{};
        const auto pixels  = data->pixels.Span<std::uint8_t>();

        for (std::size_t i = 0; i < count; ++i) {
            const auto    destination = pixels.subspan(i * pocket::rgba_bytes_per_pixel, pocket::rgba_bytes_per_pixel);
            std::uint32_t rgba        = 0;
            if (view.format == pocket::PixelFormat::Abgr8888 || view.format == pocket::PixelFormat::Indexed8) {
                const auto pixel = view.format == pocket::PixelFormat::Abgr8888
                                       ? source.subspan(i * pocket::rgba_bytes_per_pixel, pocket::rgba_bytes_per_pixel)
                                       : palette.subspan(source[i] * pocket::rgba_bytes_per_pixel, pocket::rgba_bytes_per_pixel);
                std::ranges::copy(pixel, destination.begin());
                continue;
            }

            const auto v = static_cast<std::uint32_t>(source[i * 2]) | (static_cast<std::uint32_t>(source[i * 2 + 1]) << 8);
            if (view.format == pocket::PixelFormat::Bgr565) {
                const auto r = v & 31, g = (v >> 5) & 63, b = (v >> 11) & 31;
                rgba = ((r << 3) | (r >> 2)) | (((g << 2) | (g >> 4)) << 8) | (((b << 3) | (b >> 2)) << 16) | 0xff000000u;
            } else if (view.format == pocket::PixelFormat::Abgr4444) {
                rgba = (v & 15) * 17 | (((v >> 4) & 15) * 17 << 8) | (((v >> 8) & 15) * 17 << 16) | (((v >> 12) & 15) * 17 << 24);
            } else {
                throw std::runtime_error("Unsupported PocketJS pixel format");
            }

            for (std::size_t channel = 0; channel < destination.size(); ++channel)
                destination[channel] = static_cast<std::uint8_t>(rgba >> (channel * 8));
        }
        m_Images.insert_or_assign(view.handle, Image{.revision = view.revision, .data = std::move(data)});
    }

    std::erase_if(m_Images, [&](const auto& entry) { return !live.contains(entry.first); });

    for (std::uint32_t slot = 0; slot < frame.font_slots; ++slot) {
        PocketFont view{};
        {
            const int status = pocket_font(runtime, slot, &view);
            if (status == pocket::resource_vacant) {
                m_Fonts.erase(slot);
                continue;
            }
            if (status != pocket::resource_present) throw std::runtime_error("Cannot read PocketJS font");
            const auto old = m_Fonts.find(slot);
            if (old != m_Fonts.end() && old->second.revision == view.revision) continue;
        }

        // CJK coverage needs more columns than the small Latin atlas; keeping
        // roughly square pages avoids exceeding the texture height limit.
        const auto columns = std::min(
            view.glyph_count > 4096
                ? 256u
            : view.glyph_count > 512
                ? 64u
                : 16u,
            std::max(1u, view.glyph_count));

        const auto rows = std::max(1u, (view.glyph_count + columns - 1) / columns);

        // A transparent gutter isolates glyph cells under fractional-DPI filtering.
        const auto stride_x = view.coverage_width + 2 * glyph_gutter, stride_y = view.coverage_height + 2 * glyph_gutter;
        const auto cell_bytes = static_cast<std::size_t>(view.coverage_width) * view.coverage_height;

        if (!view.coverage || view.byte_count < cell_bytes * view.glyph_count)
            throw std::runtime_error("Truncated PocketJS font atlas");

        auto       data     = NewGuiTexture(columns * stride_x, rows * stride_y, true);
        const auto coverage = std::span{view.coverage, view.byte_count};
        const auto pixels   = data->pixels.Span<std::uint8_t>();
        // Transparent white gutters avoid dark fringes with straight alpha.
        for (std::size_t i = 0; i < pixels.size(); i += pocket::rgba_bytes_per_pixel) {
            const auto pixel = pixels.subspan(i, pocket::rgba_bytes_per_pixel);
            std::ranges::fill(pixel.first<3>(), 255);
            pixel[3] = 0;
        }
        for (std::uint32_t gid = 0; gid < view.glyph_count; ++gid) {
            const auto glyph = coverage.subspan(gid * cell_bytes, cell_bytes);
            for (std::uint32_t y = 0; y < view.coverage_height; ++y) {
                for (std::uint32_t x = 0; x < view.coverage_width; ++x) {
                    const auto dst  = (static_cast<std::size_t>((gid / columns) * stride_y + glyph_gutter + y) * data->width + (gid % columns) * stride_x + glyph_gutter + x) * 4;
                    pixels[dst + 3] = glyph[y * view.coverage_width + x];
                }
            }
        }

        m_Fonts.insert_or_assign(
            slot,
            Font{
                .revision   = view.revision,
                .cell_w     = view.cell_width,
                .cell_h     = view.cell_height,
                .coverage_w = view.coverage_width,
                .coverage_h = view.coverage_height,
                .columns    = columns,
                .glyphs     = view.glyph_count,
                .data       = std::move(data),
            });
    }
}

auto PocketDrawDecoder::Textures() const -> std::vector<std::shared_ptr<const GuiTextureData>> {
    std::vector<std::shared_ptr<const GuiTextureData>> result;
    for (const auto& [_, image] : m_Images) result.push_back(image.data);
    for (const auto& [_, font] : m_Fonts) result.push_back(font.data);
    std::ranges::sort(result, {}, [](const auto& p) { return p->id; });
    return result;
}

auto PocketDrawDecoder::Build(PocketApple* runtime) -> GuiDrawList {
    PocketDrawList frame{};
    if (pocket_draw(runtime, &frame) != 0) throw std::runtime_error("Cannot build PocketJS DrawList");
    // Copy before any further runtime call; deferred draws own all their memory.
    const std::vector<std::uint32_t> words(frame.words, frame.words + frame.word_count);
    SyncResources(runtime, frame);
    GuiDrawList              list;
    math::vec4f              clip{0, 0, pocket::max_viewport_dimension, pocket::max_viewport_dimension};
    std::vector<math::vec4f> clips;
    const auto               xy = [](std::uint32_t w) -> math::vec2f {
        return {
            static_cast<float>(static_cast<std::int16_t>(w & 0xffff)),
            static_cast<float>(static_cast<std::int16_t>(w >> 16))};
    };
    const auto wh = [](std::uint32_t w) -> math::vec2f {
        return {
            static_cast<float>(w & 0xffff),
            static_cast<float>(w >> 16),
        };
    };
    const auto color = [](std::uint32_t c) -> math::Color {
        math::Color result{
            static_cast<float>(c & 0xff),
            static_cast<float>((c >> 8) & 0xff),
            static_cast<float>((c >> 16) & 0xff),
            static_cast<float>(c >> 24),
        };
        result /= 255.0f;
        return result;
    };
    const auto batch = [&](std::uint64_t texture, std::uint32_t count) {
        if (!list.commands.empty()) {
            auto& last = list.commands.back();
            if (last.texture.resource == texture && last.clip_rect == clip) {
                last.element_count += count;
                return;
            }
        }
        list.commands.emplace_back(GuiDrawCommand{
            .element_count = count,
            .index_offset  = static_cast<std::uint32_t>(list.indices.size()),
            .clip_rect     = clip,
            .texture       = {.resource = texture},
        });
    };
    const auto quad = [&](
                          math::vec2f                  pos,
                          math::vec2f                  size,
                          math::vec2f                  uv0,
                          math::vec2f                  uv1,
                          std::array<std::uint32_t, 4> colors,
                          std::uint64_t                texture) {
        if (size.x <= 0 || size.y <= 0) return;
        batch(texture, 6);
        const auto base = static_cast<std::uint32_t>(list.vertices.size());
        const auto end  = pos + size;
        list.vertices.emplace_back(GuiVertex{.position = pos, .uv = uv0, .color = color(colors[0])});
        list.vertices.emplace_back(GuiVertex{.position = {end.x, pos.y}, .uv = {uv1.x, uv0.y}, .color = color(colors[1])});
        list.vertices.emplace_back(GuiVertex{.position = end, .uv = uv1, .color = color(colors[2])});
        list.vertices.emplace_back(GuiVertex{.position = {pos.x, end.y}, .uv = {uv0.x, uv1.y}, .color = color(colors[3])});
        for (auto index : {0u, 1u, 2u, 0u, 2u, 3u})
            list.indices.emplace_back(base + index);
    };

    for (std::size_t i = 0; i < words.size();) {
        const auto length = pocket::CommandWordCount(words.data() + i, words.size() - i);
        if (!length) throw std::runtime_error("Invalid or truncated PocketJS draw command");

        const auto  op = static_cast<pocket::DrawOp>(words[i]);
        const auto* w  = words.data() + i;

        if (op == pocket::DrawOp::Rect || op == pocket::DrawOp::GradientRect) {
            std::array<std::uint32_t, 4> colors{w[3], w[3], w[3], w[3]};
            if (op == pocket::DrawOp::GradientRect) {
                const auto direction = static_cast<pocket::GradientDirection>(w[5]);
                if (direction == pocket::GradientDirection::ToTop)
                    colors = {w[4], w[4], w[3], w[3]};
                else if (direction == pocket::GradientDirection::ToLeft)
                    colors = {w[4], w[3], w[3], w[4]};
                else if (direction == pocket::GradientDirection::ToRight)
                    colors = {w[3], w[4], w[4], w[3]};
                else
                    colors = {w[3], w[3], w[4], w[4]};
            }
            quad(xy(w[1]), wh(w[2]), {}, {1, 1}, colors, 0);
        } else if (op == pocket::DrawOp::GlyphRun) {
            const auto it = m_Fonts.find(w[1] & 255);
            if (it != m_Fonts.end()) {
                const auto&       f = it->second;
                const math::vec2f atlas_size{static_cast<float>(f.data->width), static_cast<float>(f.data->height)};
                const math::vec2f coverage_size{static_cast<float>(f.coverage_w), static_cast<float>(f.coverage_h)};
                const math::vec2f cell_size{static_cast<float>(f.cell_w), static_cast<float>(f.cell_h)};
                for (std::size_t k = pocket::FixedWordCount(pocket::DrawOp::GlyphRun); k < length; k += pocket::glyph_words) {
                    const auto gid = w[k + 1] & 65535;
                    if (gid >= f.glyphs) continue;
                    const math::vec2f glyph_position{
                        static_cast<float>((gid % f.columns) * (f.coverage_w + 2 * glyph_gutter) + glyph_gutter),
                        static_cast<float>((gid / f.columns) * (f.coverage_h + 2 * glyph_gutter) + glyph_gutter),
                    };
                    const auto uv0 = glyph_position / atlas_size;
                    const auto uv1 = (glyph_position + coverage_size) / atlas_size;
                    quad(xy(w[k]), cell_size, uv0, uv1, {w[2], w[2], w[2], w[2]}, f.data->id);
                }
            }
        } else if (op == pocket::DrawOp::TextureQuad) {
            const auto it = m_Images.find(static_cast<std::int32_t>(w[1]));
            if (it != m_Images.end()) {
                quad(
                    xy(w[2]),
                    wh(w[3]), {std::bit_cast<float>(w[4]), std::bit_cast<float>(w[5])},
                    {std::bit_cast<float>(w[6]), std::bit_cast<float>(w[7])},
                    {w[8], w[8], w[8], w[8]},
                    it->second.data->id);
            }
        } else if (op == pocket::DrawOp::Scissor) {
            clips.push_back(clip);
            const auto p = xy(w[1]), s = wh(w[2]);
            const auto min_point = math::max(math::vec2f{clip.x, clip.y}, p);
            const auto max_point = math::min(math::vec2f{clip.z, clip.w}, p + s);
            clip                 = {min_point.x, min_point.y, max_point.x, max_point.y};
        } else if (op == pocket::DrawOp::ScissorPop) {
            if (clips.empty()) throw std::runtime_error("Unbalanced PocketJS clip stack");
            clip = clips.back();
            clips.pop_back();
        } else if (op == pocket::DrawOp::Triangle || op == pocket::DrawOp::TextureTriangle) {
            std::uint64_t resource = 0;
            bool          present  = true;
            if (op == pocket::DrawOp::TextureTriangle) {
                const auto it = m_Images.find(static_cast<std::int32_t>(w[1]));
                present       = it != m_Images.end();
                if (present) {
                    resource = it->second.data->id;
                }
            }
            if (present) {
                batch(resource, 3);
                for (unsigned k = 0; k < 3; ++k) {
                    list.indices.emplace_back(static_cast<std::uint32_t>(list.vertices.size()));
                    list.vertices.emplace_back(op == pocket::DrawOp::Triangle
                                                   ? GuiVertex{.position = xy(w[1 + k]), .uv = {}, .color = color(w[4 + k])}
                                                   : GuiVertex{.position = xy(w[2 + k * 3]), .uv = {std::bit_cast<float>(w[3 + k * 3]), std::bit_cast<float>(w[4 + k * 3])}, .color = color(w[11])});
                }
            }
        } else {
            // This host installs neither a native text provider nor a compositor
            // app registry. Reject these explicitly instead of silently losing UI.
            throw std::runtime_error("PocketJS native text/compositor surfaces are not registered in Hitagi");
        }
        i += length;
    }
    return list;
}
}  // namespace hitagi::gui
