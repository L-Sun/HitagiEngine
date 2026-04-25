module;

#include <ft2build.h>
#include FT_FREETYPE_H

#include <spdlog/spdlog.h>

module render;
import std;

namespace hitagi::render {
namespace {

constexpr std::uint32_t AtlasWidth   = 2048;
constexpr std::uint32_t AtlasHeight  = 2048;
constexpr std::uint32_t AtlasPadding = 1;

struct GlyphKey {
    char32_t      codepoint  = U'\0';
    std::uint32_t pixel_size = 0;

    auto operator==(const GlyphKey&) const -> bool = default;
};

struct GlyphKeyHash {
    auto operator()(GlyphKey key) const noexcept -> std::size_t {
        return std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(key.codepoint)) ^
               (std::hash<std::uint32_t>{}(key.pixel_size) << 1);
    }
};

struct GlyphInfo {
    FT_UInt glyph_index = 0;

    float advance_x = 0.0f;
    float bearing_x = 0.0f;
    float bearing_y = 0.0f;
    float width     = 0.0f;
    float height    = 0.0f;

    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;

    bool has_bitmap = false;
};

struct TextVertex {
    math::vec2f pos;
    math::vec2f uv;
    math::Color color;
};

struct FrameConstant {
    math::mat4f projection;
};

struct BindlessInfo {
    gfx::BindlessHandle frame_constant;
    gfx::BindlessHandle atlas;
    gfx::BindlessHandle sampler;
};

auto DecodeUtf8(std::string_view text) -> std::pmr::vector<char32_t> {
    std::pmr::vector<char32_t> result;
    result.reserve(text.size());

    for (std::size_t i = 0; i < text.size();) {
        const auto c0 = static_cast<unsigned char>(text[i]);
        if (c0 < 0x80) {
            result.emplace_back(static_cast<char32_t>(c0));
            ++i;
            continue;
        }

        std::uint32_t codepoint = 0;
        std::size_t   length    = 0;
        if ((c0 & 0xE0) == 0xC0) {
            codepoint = c0 & 0x1F;
            length    = 2;
        } else if ((c0 & 0xF0) == 0xE0) {
            codepoint = c0 & 0x0F;
            length    = 3;
        } else if ((c0 & 0xF8) == 0xF0) {
            codepoint = c0 & 0x07;
            length    = 4;
        } else {
            result.emplace_back(U'?');
            ++i;
            continue;
        }

        if (i + length > text.size()) {
            result.emplace_back(U'?');
            break;
        }

        bool valid = true;
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto cx = static_cast<unsigned char>(text[i + offset]);
            if ((cx & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (cx & 0x3F);
        }

        if (!valid) {
            result.emplace_back(U'?');
            ++i;
            continue;
        }

        result.emplace_back(static_cast<char32_t>(codepoint));
        i += length;
    }

    return result;
}

void AddGlyphQuad(
    std::pmr::vector<TextVertex>&    vertices,
    std::pmr::vector<std::uint32_t>& indices,
    const GlyphInfo&                 glyph,
    float                            x0,
    float                            y0,
    const math::Color&               color) {
    const auto base = static_cast<std::uint32_t>(vertices.size());
    const auto x1   = x0 + glyph.width;
    const auto y1   = y0 + glyph.height;

    vertices.emplace_back(TextVertex{.pos = {x0, y0}, .uv = {glyph.u0, glyph.v0}, .color = color});
    vertices.emplace_back(TextVertex{.pos = {x1, y0}, .uv = {glyph.u1, glyph.v0}, .color = color});
    vertices.emplace_back(TextVertex{.pos = {x1, y1}, .uv = {glyph.u1, glyph.v1}, .color = color});
    vertices.emplace_back(TextVertex{.pos = {x0, y1}, .uv = {glyph.u0, glyph.v1}, .color = color});

    indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
}

}  // namespace

struct TextRenderUtils::Impl {
    explicit Impl(gfx::Device& gfx_device, std::filesystem::path font_dir)
        : device(gfx_device),
          default_font_dir(std::move(font_dir)),
          atlas_pixels(AtlasWidth * AtlasHeight, std::byte{0}) {
        if (FT_Init_FreeType(&library) != 0) {
            spdlog::error("FreeType initialization failed; text rendering is disabled.");
            library = nullptr;
        }

        const std::pmr::string text_shader = R"""(
            #include "bindless.hlsl"

            struct BindlessInfo {
                hitagi::SimpleBuffer frame_constant;
                hitagi::Texture      atlas;
                hitagi::Sampler      sampler;
            };

            struct FrameConstant {
                matrix projection;
            };

            struct VS_INPUT {
                float2 pos : POSITION;
                float2 uv  : TEXCOORD;
                float4 col : COLOR;
            };

            struct PS_INPUT {
                float4 pos : SV_POSITION;
                float2 uv  : TEXCOORD;
                float4 col : COLOR;
            };

            PS_INPUT VSMain(VS_INPUT input) {
                BindlessInfo  bindless_info  = hitagi::load_bindless<BindlessInfo>();
                FrameConstant frame_constant = bindless_info.frame_constant.load<FrameConstant>();

                PS_INPUT output;
                output.pos = mul(frame_constant.projection, float4(input.pos.xy, 0.0f, 1.0f));
                output.uv  = input.uv;
                output.col = input.col;
                return output;
            }

            float4 PSMain(PS_INPUT input) : SV_TARGET {
                BindlessInfo bindless_info = hitagi::load_bindless<BindlessInfo>();
                SamplerState sampler       = bindless_info.sampler.load();
                float        alpha         = bindless_info.atlas.sample<float>(sampler, input.uv);
                return float4(input.col.rgb, input.col.a * alpha);
            }
        )""";

        vs = device.CreateShader({
            .name        = "text-vs",
            .type        = gfx::ShaderType::Vertex,
            .entry       = "VSMain",
            .source_code = text_shader,
        });

        ps = device.CreateShader({
            .name        = "text-ps",
            .type        = gfx::ShaderType::Pixel,
            .entry       = "PSMain",
            .source_code = text_shader,
        });

        sampler = device.CreateSampler({
            .name           = "text-sampler",
            .address_u      = gfx::AddressMode::Clamp,
            .address_v      = gfx::AddressMode::Clamp,
            .address_w      = gfx::AddressMode::Clamp,
            .mag_filter     = gfx::FilterMode::Linear,
            .min_filter     = gfx::FilterMode::Linear,
            .mipmap_filter  = gfx::FilterMode::Linear,
            .min_lod        = 0,
            .max_lod        = 0,
            .max_anisotropy = 1,
            .compare_op     = gfx::CompareOp::Always,
        });

        pipeline = device.CreateRenderPipeline({
            .name           = "text",
            .shaders        = {vs, ps},
            .assembly_state = {
                .primitive = gfx::PrimitiveTopology::TriangleList,
            },
            .vertex_input_layout = {
                {"POSITION", gfx::Format::R32G32_FLOAT, 0, offsetof(TextVertex, pos), sizeof(TextVertex)},
                {"TEXCOORD", gfx::Format::R32G32_FLOAT, 0, offsetof(TextVertex, uv), sizeof(TextVertex)},
                {"COLOR", gfx::Format::R32G32B32A32_FLOAT, 0, offsetof(TextVertex, color), sizeof(TextVertex)},
            },
            .rasterization_state = {
                .cull_mode               = gfx::CullMode::None,
                .front_counter_clockwise = false,
            },
            .blend_state = {
                .blend_enable           = true,
                .src_color_blend_factor = gfx::BlendFactor::SrcAlpha,
                .dst_color_blend_factor = gfx::BlendFactor::InvSrcAlpha,
                .color_blend_op         = gfx::BlendOp::Add,
                .src_alpha_blend_factor = gfx::BlendFactor::One,
                .dst_alpha_blend_factor = gfx::BlendFactor::InvSrcAlpha,
                .alpha_blend_op         = gfx::BlendOp::Add,
            },
            .render_format = gfx::Format::R8G8B8A8_UNORM,
        });
    }

    ~Impl() {
        if (face != nullptr) {
            FT_Done_Face(face);
        }
        if (library != nullptr) {
            FT_Done_FreeType(library);
        }
    }

    void TextPass(rg::RenderGraph& render_graph, rg::TextureHandle target, std::span<const TextDrawCommand> commands, bool clear_target) {
        if (commands.empty() || !EnsureFace()) return;

        std::pmr::vector<TextVertex>    vertices;
        std::pmr::vector<std::uint32_t> indices;
        BuildLayout(commands, vertices, indices);

        if (vertices.empty() || indices.empty()) return;
        UploadAtlasIfNeeded();
        if (atlas_texture == nullptr) return;

        const auto bindless_info_handle = render_graph.Create(
            {
                .name          = "text_bindless_info",
                .element_size  = sizeof(BindlessInfo),
                .element_count = 1,
                .usages        = gfx::GPUBufferUsageFlags::Constant | gfx::GPUBufferUsageFlags::MapWrite,
            },
            "text_bindless_info");

        const auto frame_constant_handle = render_graph.Create(
            {
                .name         = "text_frame_constant",
                .element_size = sizeof(FrameConstant),
                .usages       = gfx::GPUBufferUsageFlags::Constant | gfx::GPUBufferUsageFlags::MapWrite,
            },
            "text_frame_constant");

        const auto vertex_buffer_handle = render_graph.Create(
            gfx::GPUBufferDesc{
                .name          = "text_vertices",
                .element_size  = sizeof(TextVertex),
                .element_count = static_cast<std::uint64_t>(vertices.size()),
                .usages        = gfx::GPUBufferUsageFlags::Vertex | gfx::GPUBufferUsageFlags::MapWrite,
            },
            "text_vertices");

        const auto index_buffer_handle = render_graph.Create(
            gfx::GPUBufferDesc{
                .name          = "text_indices",
                .element_size  = sizeof(std::uint32_t),
                .element_count = static_cast<std::uint64_t>(indices.size()),
                .usages        = gfx::GPUBufferUsageFlags::Index | gfx::GPUBufferUsageFlags::MapWrite,
            },
            "text_indices");

        const auto atlas_name      = std::format("text_atlas_{}", atlas_generation);
        const auto atlas_handle    = render_graph.Import(atlas_texture, atlas_name);
        const auto sampler_handle  = render_graph.Import(sampler, "text_sampler");
        const auto pipeline_handle = render_graph.Import(pipeline, "text_pipeline");

        rg::RenderPassBuilder builder(render_graph);
        builder.SetName("TextRenderPass")
            .Read(bindless_info_handle)
            .Read(frame_constant_handle)
            .ReadAsVertices(vertex_buffer_handle)
            .ReadAsIndices(index_buffer_handle)
            .Read(atlas_handle, {}, gfx::PipelineStage::PixelShader)
            .AddSampler(sampler_handle)
            .AddPipeline(pipeline_handle)
            .SetRenderTarget(target, clear_target);

        builder.SetExecutor(
                   [=, vertices = std::move(vertices), indices = std::move(indices)](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
                       auto& cmd           = pass.GetCmd();
                       auto& render_target = pass.Resolve(target);

                       cmd.SetViewPort({
                           .x      = 0.0f,
                           .y      = 0.0f,
                           .width  = static_cast<float>(render_target.GetDesc().width),
                           .height = static_cast<float>(render_target.GetDesc().height),
                       });
                       cmd.SetScissorRect({
                           .x      = 0,
                           .y      = 0,
                           .width  = render_target.GetDesc().width,
                           .height = render_target.GetDesc().height,
                       });

                       gfx::GPUBufferView<FrameConstant> frame_constant(pass.Resolve(frame_constant_handle));
                       frame_constant.front().projection = math::ortho(
                           0.0f,
                           static_cast<float>(render_target.GetDesc().width),
                           static_cast<float>(render_target.GetDesc().height),
                           0.0f,
                           3.0f,
                           -1.0f);

                       gfx::GPUBufferView<TextVertex> vertex_buffer(pass.Resolve(vertex_buffer_handle));
                       std::memcpy(vertex_buffer.data(), vertices.data(), vertices.size() * sizeof(TextVertex));

                       gfx::GPUBufferView<std::uint32_t> index_buffer(pass.Resolve(index_buffer_handle));
                       std::memcpy(index_buffer.data(), indices.data(), indices.size() * sizeof(std::uint32_t));

                       gfx::GPUBufferView<BindlessInfo> bindless_infos(pass.Resolve(bindless_info_handle));
                       bindless_infos.front() = {
                           .frame_constant = pass.GetBindless(frame_constant_handle),
                           .atlas          = pass.GetBindless(atlas_handle),
                           .sampler        = pass.GetBindless(sampler_handle),
                       };

                       cmd.SetPipeline(pass.Resolve(pipeline_handle));
                       cmd.SetVertexBuffers(0, {{pass.Resolve(vertex_buffer_handle)}}, {{0}});
                       cmd.SetIndexBuffer(pass.Resolve(index_buffer_handle));
                       cmd.PushBindlessMetaInfo({
                           .handle = pass.GetBindless(bindless_info_handle),
                       });
                       cmd.DrawIndexed(static_cast<std::uint32_t>(indices.size()));
                   })
            .Finish();
    }

    bool EnsureFace() {
        if (face != nullptr) return true;
        if (library == nullptr || tried_load_face) return false;
        tried_load_face = true;

        const auto path = FindDefaultFont();
        if (path.empty()) {
            spdlog::warn("No default font found under '{}'; text rendering is disabled.", default_font_dir.string());
            return false;
        }

        const auto path_string = path.string();
        if (FT_New_Face(library, path_string.c_str(), 0, &face) != 0) {
            spdlog::warn("Failed to load font '{}'; text rendering is disabled.", path_string);
            face = nullptr;
            return false;
        }

        spdlog::info("Text renderer loaded default font '{}'.", path_string);
        return true;
    }

    auto FindDefaultFont() const -> std::filesystem::path {
        constexpr std::array candidates = {
            "NotoSansSC-Regular.otf",
            "Hasklig-Regular.otf",
            "NotoSansJP-Regular.otf",
        };

        for (const auto* candidate : candidates) {
            auto path = default_font_dir / candidate;
            if (std::filesystem::is_regular_file(path)) {
                return path;
            }
        }

        if (!std::filesystem::is_directory(default_font_dir)) {
            return {};
        }

        for (const auto& entry : std::filesystem::directory_iterator(default_font_dir)) {
            if (!entry.is_regular_file()) continue;
            const auto extension = entry.path().extension().string();
            if (extension == ".ttf" || extension == ".otf") {
                return entry.path();
            }
        }

        return {};
    }

    bool EnsurePixelSize(std::uint32_t pixel_size) {
        pixel_size = std::max<std::uint32_t>(1, pixel_size);
        if (current_pixel_size == pixel_size) return true;
        if (!EnsureFace()) return false;

        if (FT_Set_Pixel_Sizes(face, 0, pixel_size) != 0) {
            spdlog::warn("Failed to set font pixel size {}.", pixel_size);
            return false;
        }

        current_pixel_size = pixel_size;
        return true;
    }

    auto GetAscender(std::uint32_t pixel_size) -> float {
        if (!EnsurePixelSize(pixel_size)) return static_cast<float>(pixel_size);
        return static_cast<float>(face->size->metrics.ascender) / 64.0f;
    }

    auto GetLineHeight(std::uint32_t pixel_size) -> float {
        if (!EnsurePixelSize(pixel_size)) return static_cast<float>(pixel_size) * 1.2f;
        return static_cast<float>(face->size->metrics.height) / 64.0f;
    }

    auto GetKerning(FT_UInt previous, FT_UInt current, std::uint32_t pixel_size) -> float {
        if (previous == 0 || current == 0 || !FT_HAS_KERNING(face) || !EnsurePixelSize(pixel_size)) {
            return 0.0f;
        }

        FT_Vector kerning{};
        if (FT_Get_Kerning(face, previous, current, FT_KERNING_DEFAULT, &kerning) != 0) {
            return 0.0f;
        }
        return static_cast<float>(kerning.x) / 64.0f;
    }

    auto GetGlyph(char32_t codepoint, std::uint32_t pixel_size) -> const GlyphInfo* {
        const auto key = GlyphKey{.codepoint = codepoint, .pixel_size = pixel_size};
        if (const auto it = glyphs.find(key); it != glyphs.end()) {
            return &it->second;
        }

        if (!EnsurePixelSize(pixel_size)) return nullptr;

        FT_UInt glyph_index = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
        if (glyph_index == 0 && codepoint != U'?') {
            glyph_index = FT_Get_Char_Index(face, static_cast<FT_ULong>(U'?'));
        }
        if (glyph_index == 0) return nullptr;

        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) != 0) {
            return nullptr;
        }

        const auto* slot   = face->glyph;
        const auto& bitmap = slot->bitmap;

        GlyphInfo glyph{
            .glyph_index = glyph_index,
            .advance_x   = static_cast<float>(slot->advance.x) / 64.0f,
            .bearing_x   = static_cast<float>(slot->bitmap_left),
            .bearing_y   = static_cast<float>(slot->bitmap_top),
            .width       = static_cast<float>(bitmap.width),
            .height      = static_cast<float>(bitmap.rows),
            .has_bitmap  = bitmap.width != 0 && bitmap.rows != 0,
        };

        if (glyph.has_bitmap) {
            if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
                spdlog::warn("Unsupported FreeType bitmap pixel mode {}, glyph skipped.", bitmap.pixel_mode);
                return nullptr;
            }

            const auto [atlas_x, atlas_y] = AllocateAtlasRegion(bitmap.width, bitmap.rows);
            CopyGlyphBitmap(bitmap, atlas_x, atlas_y);

            glyph.u0    = static_cast<float>(atlas_x) / static_cast<float>(AtlasWidth);
            glyph.v0    = static_cast<float>(atlas_y) / static_cast<float>(AtlasHeight);
            glyph.u1    = static_cast<float>(atlas_x + bitmap.width) / static_cast<float>(AtlasWidth);
            glyph.v1    = static_cast<float>(atlas_y + bitmap.rows) / static_cast<float>(AtlasHeight);
            atlas_dirty = true;
        }

        auto [it, _] = glyphs.emplace(key, glyph);
        return &it->second;
    }

    auto AllocateAtlasRegion(std::uint32_t width, std::uint32_t height) -> std::pair<std::uint32_t, std::uint32_t> {
        const auto padded_width  = width + AtlasPadding * 2;
        const auto padded_height = height + AtlasPadding * 2;

        if (atlas_pen_x + padded_width > AtlasWidth) {
            atlas_pen_x = AtlasPadding;
            atlas_pen_y += atlas_row_height;
            atlas_row_height = 0;
        }

        if (atlas_pen_y + padded_height > AtlasHeight) {
            throw std::runtime_error("text glyph atlas is full");
        }

        const auto x = atlas_pen_x + AtlasPadding;
        const auto y = atlas_pen_y + AtlasPadding;
        atlas_pen_x += padded_width;
        atlas_row_height = std::max(atlas_row_height, padded_height);
        return {x, y};
    }

    void CopyGlyphBitmap(const FT_Bitmap& bitmap, std::uint32_t atlas_x, std::uint32_t atlas_y) {
        for (std::uint32_t row = 0; row < bitmap.rows; ++row) {
            const auto* src = bitmap.pitch >= 0
                                  ? bitmap.buffer + row * bitmap.pitch
                                  : bitmap.buffer + (bitmap.rows - 1 - row) * static_cast<std::uint32_t>(-bitmap.pitch);
            auto*       dst = atlas_pixels.data() + (atlas_y + row) * AtlasWidth + atlas_x;
            std::memcpy(dst, src, bitmap.width);
        }
    }

    void UploadAtlasIfNeeded() {
        if (atlas_texture != nullptr && !atlas_dirty) return;

        atlas_texture = device.CreateTexture(
            {
                .name   = "text-atlas",
                .width  = AtlasWidth,
                .height = AtlasHeight,
                .format = gfx::Format::R8_UNORM,
                .usages = gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopyDst,
            },
            atlas_pixels);
        ++atlas_generation;
        atlas_dirty = false;
    }

    void BuildLayout(
        std::span<const TextDrawCommand> commands,
        std::pmr::vector<TextVertex>&    vertices,
        std::pmr::vector<std::uint32_t>& indices) {
        for (const auto& command : commands) {
            const auto pixel_size  = static_cast<std::uint32_t>(std::max(1.0f, std::round(command.font_size)));
            const auto ascender    = GetAscender(pixel_size);
            const auto line_height = GetLineHeight(pixel_size);
            auto       pen_x       = command.position.x;
            auto       baseline_y  = command.position.y + ascender;
            FT_UInt    previous    = 0;

            for (const auto codepoint : DecodeUtf8(command.text)) {
                if (codepoint == U'\n') {
                    pen_x = command.position.x;
                    baseline_y += line_height;
                    previous = 0;
                    continue;
                }

                const auto* glyph = GetGlyph(codepoint, pixel_size);
                if (glyph == nullptr) {
                    previous = 0;
                    continue;
                }

                pen_x += GetKerning(previous, glyph->glyph_index, pixel_size);

                if (glyph->has_bitmap) {
                    const auto x0 = pen_x + glyph->bearing_x;
                    const auto y0 = baseline_y - glyph->bearing_y;
                    AddGlyphQuad(vertices, indices, *glyph, x0, y0, command.color);
                }

                pen_x += glyph->advance_x;
                previous = glyph->glyph_index;
            }
        }
    }

    gfx::Device&          device;
    std::filesystem::path default_font_dir;

    FT_Library library = nullptr;
    FT_Face    face    = nullptr;

    bool          tried_load_face    = false;
    std::uint32_t current_pixel_size = 0;

    std::pmr::vector<std::byte> atlas_pixels;
    std::uint32_t               atlas_pen_x      = AtlasPadding;
    std::uint32_t               atlas_pen_y      = AtlasPadding;
    std::uint32_t               atlas_row_height = 0;
    bool                        atlas_dirty      = false;

    std::pmr::unordered_map<GlyphKey, GlyphInfo, GlyphKeyHash> glyphs;

    std::shared_ptr<gfx::Shader>         vs;
    std::shared_ptr<gfx::Shader>         ps;
    std::shared_ptr<gfx::RenderPipeline> pipeline;
    std::shared_ptr<gfx::Sampler>        sampler;
    std::shared_ptr<gfx::Texture>        atlas_texture;
    std::uint64_t                        atlas_generation = 0;
};

TextRenderUtils::TextRenderUtils(gfx::Device& gfx_device, std::filesystem::path font_dir)
    : m_Impl(std::make_unique<Impl>(gfx_device, std::move(font_dir))) {}

TextRenderUtils::~TextRenderUtils() = default;

void TextRenderUtils::TextPass(rg::RenderGraph& render_graph, rg::TextureHandle target, std::span<const TextDrawCommand> commands, bool clear_target) {
    m_Impl->TextPass(render_graph, target, commands, clear_target);
}

}  // namespace hitagi::render
