export module gui:pocket_draw;
import interop.pocketjs;
import std;
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
    void                                    SyncResources(PocketApple* runtime, const PocketHitagiDrawList& frame);
};
}  // namespace hitagi::gui

namespace hitagi::gui {
auto NewGuiTexture(std::uint32_t width, std::uint32_t height, bool linear) -> std::shared_ptr<GuiTextureData> {
    static std::atomic<std::uint64_t> next_id{1};
    if (!width || !height || width > 16384 || height > 16384 || std::uint64_t(width) * height > 64u * 1024u * 1024u)
        throw std::runtime_error("Invalid PocketJS texture dimensions");
    auto data    = std::make_shared<GuiTextureData>();
    data->id     = next_id.fetch_add(1, std::memory_order_relaxed);
    data->width  = width;
    data->height = height;
    data->linear = linear;
    data->pixels.resize(std::size_t(width) * height * 4);
    return data;
}

void PocketDrawDecoder::SyncResources(PocketApple* runtime, const PocketHitagiDrawList& frame) {
    std::unordered_set<std::int32_t> live;
    for (std::uint32_t slot = 0; slot < frame.texture_slots; ++slot) {
        PocketHitagiTexture view{};
        const int           status = pocket_hitagi_texture(runtime, slot, &view);
        if (status == 1) continue;
        if (status != 0) throw std::runtime_error("Cannot read PocketJS texture");
        live.insert(view.handle);
        const auto old = m_Images.find(view.handle);
        if (old != m_Images.end() && old->second.revision == view.revision) continue;
        auto              data            = NewGuiTexture(view.width, view.height, view.linear != 0);
        const std::size_t count           = std::size_t(view.width) * view.height;
        const std::size_t bytes_per_pixel = view.format == 3 ? 4 : view.format == 5 ? 1
                                                                                    : 2;
        if (!view.pixels || view.byte_count < count * bytes_per_pixel ||
            (view.format == 5 && (!view.palette || view.palette_bytes < 1024)))
            throw std::runtime_error("Truncated PocketJS texture");
        for (std::size_t i = 0; i < count; ++i) {
            std::uint32_t rgba = 0;
            if (view.format == 3 || view.format == 5) {
                const auto* p = view.format == 3 ? view.pixels + i * 4 : view.palette + view.pixels[i] * 4;
                std::memcpy(data->pixels.data() + i * 4, p, 4);
                continue;
            }
            const auto v = std::uint32_t(view.pixels[i * 2]) | (std::uint32_t(view.pixels[i * 2 + 1]) << 8);
            if (view.format == 0) {
                const auto r = v & 31, g = (v >> 5) & 63, b = (v >> 11) & 31;
                rgba = ((r << 3) | (r >> 2)) | (((g << 2) | (g >> 4)) << 8) | (((b << 3) | (b >> 2)) << 16) | 0xff000000u;
            } else if (view.format == 2) {
                rgba = (v & 15) * 17 | (((v >> 4) & 15) * 17 << 8) | (((v >> 8) & 15) * 17 << 16) | (((v >> 12) & 15) * 17 << 24);
            } else
                throw std::runtime_error("Unsupported PocketJS pixel format");
            std::memcpy(data->pixels.data() + i * 4, &rgba, 4);
        }
        m_Images.insert_or_assign(view.handle, Image{view.revision, std::move(data)});
    }
    std::erase_if(m_Images, [&](const auto& entry) { return !live.contains(entry.first); });
    for (std::uint32_t slot = 0; slot < frame.font_slots; ++slot) {
        PocketHitagiFont view{};
        const int        status = pocket_hitagi_font(runtime, slot, &view);
        if (status == 1) {
            m_Fonts.erase(slot);
            continue;
        }
        if (status != 0) throw std::runtime_error("Cannot read PocketJS font");
        const auto old = m_Fonts.find(slot);
        if (old != m_Fonts.end() && old->second.revision == view.revision) continue;
        // CJK coverage needs more columns than the small Latin atlas; keeping
        // roughly square pages avoids exceeding the texture height limit.
        const auto columns = std::min(view.glyph_count > 4096 ? 256u : view.glyph_count > 512 ? 64u : 16u, std::max(1u, view.glyph_count));
        const auto rows    = std::max(1u, (view.glyph_count + columns - 1) / columns);
        // A transparent gutter isolates glyph cells under fractional-DPI filtering.
        const auto stride_x = view.coverage_width + 2, stride_y = view.coverage_height + 2;
        auto       data       = NewGuiTexture(columns * stride_x, rows * stride_y, true);
        const auto cell_bytes = std::size_t(view.coverage_width) * view.coverage_height;
        if (!view.coverage || view.byte_count < cell_bytes * view.glyph_count)
            throw std::runtime_error("Truncated PocketJS font atlas");
        for (std::uint32_t gid = 0; gid < view.glyph_count; ++gid)
            for (std::uint32_t y = 0; y < view.coverage_height; ++y)
                for (std::uint32_t x = 0; x < view.coverage_width; ++x) {
                    const auto dst    = (std::size_t((gid / columns) * stride_y + 1 + y) * data->width + (gid % columns) * stride_x + 1 + x) * 4;
                    data->pixels[dst] = data->pixels[dst + 1] = data->pixels[dst + 2] = std::byte{255};
                    data->pixels[dst + 3]                                             = std::byte{view.coverage[gid * cell_bytes + y * view.coverage_width + x]};
                }
        // Keep transparent gutters white too, avoiding dark fringes with straight alpha.
        for (std::size_t i = 0; i < data->pixels.size(); i += 4)
            data->pixels[i] = data->pixels[i + 1] = data->pixels[i + 2] = std::byte{255};
        m_Fonts.insert_or_assign(slot, Font{view.revision, view.cell_width, view.cell_height,
                                            view.coverage_width, view.coverage_height, columns, view.glyph_count, std::move(data)});
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
    PocketHitagiDrawList frame{};
    if (pocket_hitagi_draw(runtime, &frame) != 0) throw std::runtime_error("Cannot build PocketJS DrawList");
    // Copy before any further runtime call; deferred draws own all their memory.
    const std::vector<std::uint32_t> words(frame.words, frame.words + frame.word_count);
    SyncResources(runtime, frame);
    GuiDrawList              list;
    math::vec4f              clip{0, 0, 8192, 8192};
    std::vector<math::vec4f> clips;
    const auto               xy    = [](std::uint32_t w) -> math::vec2f { return {float(std::int16_t(w & 0xffff)), float(std::int16_t(w >> 16))}; };
    const auto               wh    = [](std::uint32_t w) -> math::vec2f { return {float(w & 0xffff), float(w >> 16)}; };
    const auto               color = [](std::uint32_t c) -> math::Color { return {float(c & 255) / 255, float((c >> 8) & 255) / 255, float((c >> 16) & 255) / 255, float(c >> 24) / 255}; };
    const auto               batch = [&](std::uint64_t texture, std::uint32_t count) {
        if (!list.commands.empty()) {
            auto& last = list.commands.back();
            if (last.texture.resource == texture && last.clip_rect == clip) {
                last.element_count += count;
                return;
            }
        }
        list.commands.push_back({.element_count = count, .index_offset = static_cast<std::uint32_t>(list.indices.size()), .clip_rect = clip, .texture = {.resource = texture}});
    };
    const auto quad = [&](math::vec2f pos, math::vec2f size, math::vec2f uv0, math::vec2f uv1,
                          std::array<std::uint32_t, 4> colors, std::uint64_t texture) {
        if (size.x <= 0 || size.y <= 0) return;
        batch(texture, 6);
        const auto base = static_cast<std::uint32_t>(list.vertices.size());
        list.vertices.push_back({pos, uv0, color(colors[0])});
        list.vertices.push_back({{pos.x + size.x, pos.y}, {uv1.x, uv0.y}, color(colors[1])});
        list.vertices.push_back({{pos.x + size.x, pos.y + size.y}, uv1, color(colors[2])});
        list.vertices.push_back({{pos.x, pos.y + size.y}, {uv0.x, uv1.y}, color(colors[3])});
        for (auto index : {0u, 1u, 2u, 0u, 2u, 3u}) list.indices.push_back(base + index);
    };
    for (std::size_t i = 0; i < words.size();) {
        const auto        op      = words[i];
        const std::size_t fixed[] = {0, 4, 6, 3, 9, 3, 1, 7, 12, 8, 9};
        if (op == 0 || op > 10 || words.size() - i < fixed[op]) throw std::runtime_error("Invalid PocketJS draw opcode");
        std::size_t length = fixed[op];
        if (op == 3) length += 2 * std::size_t(words[i + 1] >> 16);
        if (op == 9) length += (std::size_t(words[i + 7]) + 3) / 4;
        if (length > words.size() - i) throw std::runtime_error("Truncated PocketJS DrawList");
        const auto* w = words.data() + i;
        if (op == 1 || op == 2) {
            std::array<std::uint32_t, 4> colors{w[3], w[3], w[3], w[3]};
            if (op == 2) {
                if (w[5] == 0)
                    colors = {w[4], w[4], w[3], w[3]};
                else if (w[5] == 2)
                    colors = {w[4], w[3], w[3], w[4]};
                else if (w[5] == 3)
                    colors = {w[3], w[4], w[4], w[3]};
                else
                    colors = {w[3], w[3], w[4], w[4]};
            }
            quad(xy(w[1]), wh(w[2]), {}, {1, 1}, colors, 0);
        } else if (op == 3) {
            const auto it = m_Fonts.find(w[1] & 255);
            if (it != m_Fonts.end()) {
                const auto& f = it->second;
                for (std::size_t k = 3; k < length; k += 2) {
                    const auto gid = w[k + 1] & 65535;
                    if (gid >= f.glyphs) continue;
                    const float x = float((gid % f.columns) * (f.coverage_w + 2) + 1), y = float((gid / f.columns) * (f.coverage_h + 2) + 1);
                    quad(xy(w[k]), {float(f.cell_w), float(f.cell_h)}, {x / f.data->width, y / f.data->height},
                         {(x + f.coverage_w) / f.data->width, (y + f.coverage_h) / f.data->height}, {w[2], w[2], w[2], w[2]}, f.data->id);
                }
            }
        } else if (op == 4) {
            const auto it = m_Images.find(static_cast<std::int32_t>(w[1]));
            if (it != m_Images.end()) quad(xy(w[2]), wh(w[3]), {std::bit_cast<float>(w[4]), std::bit_cast<float>(w[5])},
                                           {std::bit_cast<float>(w[6]), std::bit_cast<float>(w[7])}, {w[8], w[8], w[8], w[8]}, it->second.data->id);
        } else if (op == 5) {
            clips.push_back(clip);
            const auto p = xy(w[1]), s = wh(w[2]);
            clip = {std::max(clip.x, p.x), std::max(clip.y, p.y), std::min(clip.z, p.x + s.x), std::min(clip.w, p.y + s.y)};
        } else if (op == 6) {
            if (clips.empty()) throw std::runtime_error("Unbalanced PocketJS clip stack");
            clip = clips.back();
            clips.pop_back();
        } else if (op == 7 || op == 8) {
            std::uint64_t resource = 0;
            bool          present  = true;
            if (op == 8) {
                const auto it = m_Images.find(static_cast<std::int32_t>(w[1]));
                present       = it != m_Images.end();
                if (present) resource = it->second.data->id;
            }
            if (present) {
                batch(resource, 3);
                for (unsigned k = 0; k < 3; ++k) {
                    list.indices.push_back(static_cast<std::uint32_t>(list.vertices.size()));
                    list.vertices.push_back(op == 7 ? GuiVertex{xy(w[1 + k]), {}, color(w[4 + k])} : GuiVertex{xy(w[2 + k * 3]), {std::bit_cast<float>(w[3 + k * 3]), std::bit_cast<float>(w[4 + k * 3])}, color(w[11])});
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
