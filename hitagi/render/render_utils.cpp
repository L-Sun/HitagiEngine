module;

#include <spdlog/spdlog.h>

module render;
import std;

namespace hitagi::render {

namespace {

struct GuiFrameConstant {
    math::mat4f orth;
};

struct GuiBindlessInfo {
    gfx::BindlessHandle frame_constant;
    gfx::BindlessHandle texture;
    gfx::BindlessHandle sampler;
};

auto CountDrawCalls(const gui::GuiDrawData& draw_data) noexcept -> std::size_t {
    std::size_t result = 0;
    for (const auto& draw_list : draw_data.draw_lists) {
        result += draw_list.commands.size();
    }
    return result;
}

auto CountVertices(const gui::GuiDrawData& draw_data) noexcept -> std::size_t {
    std::size_t result = 0;
    for (const auto& draw_list : draw_data.draw_lists) {
        result += draw_list.vertices.size();
    }
    return result;
}

auto CountIndices(const gui::GuiDrawData& draw_data) noexcept -> std::size_t {
    std::size_t result = 0;
    for (const auto& draw_list : draw_data.draw_lists) {
        result += draw_list.indices.size();
    }
    return result;
}

void AddUniqueTexture(std::pmr::vector<rg::TextureHandle>& textures, rg::TextureHandle texture) {
    if (!texture) return;
    if (std::ranges::find(textures, texture) == textures.end()) {
        textures.emplace_back(texture);
    }
}

auto CollectTextures(const gui::GuiDrawData& draw_data) -> std::pmr::vector<rg::TextureHandle> {
    std::pmr::vector<rg::TextureHandle> textures;
    for (const auto& draw_list : draw_data.draw_lists) {
        for (const auto& command : draw_list.commands) {
            if (command.texture.type == gui::GuiTextureRef::Type::RenderGraph) {
                AddUniqueTexture(textures, command.texture.texture);
            }
        }
    }
    return textures;
}

}  // namespace

GuiRenderUtils::GuiRenderUtils(gfx::Device& gfx_device) {
    const std::pmr::string gui_shader = R"""(
        #include "bindless.hlsl"
        struct BindlessInfo {
            hitagi::SimpleBuffer frame_constant;
            hitagi::Texture      texture;
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
            output.pos = mul(frame_constant.projection, float4(input.pos.xy, 0.f, 1.f));
            output.col = input.col;
            output.uv  = input.uv;
            return output;
        }

        float4 PSMain(PS_INPUT input) : SV_TARGET {
            BindlessInfo bindless_info = hitagi::load_bindless<BindlessInfo>();
            SamplerState sampler       = bindless_info.sampler.load();
            float4       out_col       = input.col * bindless_info.texture.sample<float4>(sampler, input.uv);
            return out_col;
        }
)""";

    m_GfxData.vs = gfx_device.CreateShader({
        .name        = "gui-vs",
        .type        = gfx::ShaderType::Vertex,
        .entry       = "VSMain",
        .source_code = gui_shader,
    });

    m_GfxData.ps = gfx_device.CreateShader({
        .name        = "gui-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSMain",
        .source_code = gui_shader,
    });

    m_GfxData.sampler = gfx_device.CreateSampler({
        .name           = "gui-sampler",
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

    m_GfxData.pipeline = gfx_device.CreateRenderPipeline({
        .name           = "gui",
        .shaders        = {m_GfxData.vs, m_GfxData.ps},
        .assembly_state = {
            .primitive = gfx::PrimitiveTopology::TriangleList,
        },
        .vertex_input_layout = {
            {"POSITION", gfx::Format::R32G32_FLOAT, 0, offsetof(gui::GuiVertex, position), sizeof(gui::GuiVertex)},
            {"TEXCOORD", gfx::Format::R32G32_FLOAT, 0, offsetof(gui::GuiVertex, uv), sizeof(gui::GuiVertex)},
            {"COLOR", gfx::Format::R32G32B32A32_FLOAT, 0, offsetof(gui::GuiVertex, color), sizeof(gui::GuiVertex)},
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

void GuiRenderUtils::GuiPass(rg::RenderGraph& render_graph, rg::TextureHandle target, const gui::GuiDrawData& draw_data, bool clear_target) {
    if (draw_data.Empty() || draw_data.font_atlas.Empty()) return;

    const auto total_vertices = CountVertices(draw_data);
    const auto total_indices  = CountIndices(draw_data);
    const auto total_draws    = CountDrawCalls(draw_data);
    if (total_vertices == 0 || total_indices == 0 || total_draws == 0) return;

    if (m_GfxData.font_texture == nullptr || m_FontTextureGeneration != draw_data.font_atlas.generation) {
        m_GfxData.font_texture = render_graph.GetDevice().CreateTexture(
            {
                .name   = "gui-font",
                .width  = draw_data.font_atlas.width,
                .height = draw_data.font_atlas.height,
                .format = gfx::Format::R8G8B8A8_UNORM,
                .usages = gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopyDst,
            },
            draw_data.font_atlas.pixels);
        m_FontTextureGeneration = draw_data.font_atlas.generation;
    }

    const auto bindless_info_handle = render_graph.Create(
        {
            .name          = "gui_bindless_info",
            .element_size  = sizeof(GuiBindlessInfo),
            .element_count = total_draws,
            .usages        = gfx::GPUBufferUsageFlags::Constant | gfx::GPUBufferUsageFlags::MapWrite,
        },
        "gui_bindless_info");

    const auto frame_constant_handle = render_graph.Create(
        {
            .name         = "gui_frame_constant",
            .element_size = sizeof(GuiFrameConstant),
            .usages       = gfx::GPUBufferUsageFlags::Constant | gfx::GPUBufferUsageFlags::MapWrite,
        },
        "gui_frame_constant");

    const auto vertex_buffer_handle = render_graph.Create(
        gfx::GPUBufferDesc{
            .element_size  = sizeof(gui::GuiVertex),
            .element_count = static_cast<std::uint64_t>(total_vertices),
            .usages        = gfx::GPUBufferUsageFlags::Vertex | gfx::GPUBufferUsageFlags::MapWrite,
        },
        "gui_vertices");

    const auto index_buffer_handle = render_graph.Create(
        gfx::GPUBufferDesc{
            .element_size  = sizeof(std::uint32_t),
            .element_count = static_cast<std::uint64_t>(total_indices),
            .usages        = gfx::GPUBufferUsageFlags::Index | gfx::GPUBufferUsageFlags::MapWrite,
        },
        "gui_indices");

    const auto font_texture_handle = render_graph.Import(m_GfxData.font_texture, std::format("gui_font_{}", m_FontTextureGeneration));
    const auto sampler_handle      = render_graph.Import(m_GfxData.sampler, "gui_sampler");
    const auto pipeline_handle     = render_graph.Import(m_GfxData.pipeline, "gui_pipeline");
    auto       read_textures       = CollectTextures(draw_data);

    rg::RenderPassBuilder builder(render_graph);
    builder.SetName("GuiRenderPass")
        .Read(bindless_info_handle)
        .Read(frame_constant_handle)
        .ReadAsVertices(vertex_buffer_handle)
        .ReadAsIndices(index_buffer_handle)
        .Read(font_texture_handle, {}, gfx::PipelineStage::PixelShader)
        .AddSampler(sampler_handle)
        .AddPipeline(pipeline_handle)
        .SetRenderTarget(target, clear_target);

    for (const auto texture : read_textures) {
        builder.Read(texture, {}, gfx::PipelineStage::PixelShader);
    }

    builder.SetExecutor([=, draw_data = &draw_data](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
               auto& cmd = pass.GetCmd();

               auto& vertex_buffer      = pass.Resolve(vertex_buffer_handle);
               auto  vertex_buffer_view = gfx::GPUBufferView<gui::GuiVertex>(vertex_buffer);

               auto& index_buffer      = pass.Resolve(index_buffer_handle);
               auto  index_buffer_view = gfx::GPUBufferView<std::uint32_t>(index_buffer);

               const auto frame_constant_bindless = pass.GetBindless(frame_constant_handle);
               {
                   gfx::GPUBufferView<GuiFrameConstant> frame_constant(pass.Resolve(frame_constant_handle));
                   frame_constant.front().orth = math::ortho(
                       draw_data->display_pos.x,
                       draw_data->display_pos.x + draw_data->display_size.x,
                       draw_data->display_pos.y + draw_data->display_size.y,
                       draw_data->display_pos.y,
                       3.0f,
                       -1.0f);
               }
               const auto sampler_bindless = pass.GetBindless(sampler_handle);

               std::size_t vertex_offset = 0;
               std::size_t index_offset  = 0;
               for (const auto& draw_list : draw_data->draw_lists) {
                   std::memcpy(vertex_buffer_view.data() + vertex_offset, draw_list.vertices.data(), draw_list.vertices.size() * sizeof(gui::GuiVertex));
                   std::memcpy(index_buffer_view.data() + index_offset, draw_list.indices.data(), draw_list.indices.size() * sizeof(std::uint32_t));
                   vertex_offset += draw_list.vertices.size();
                   index_offset += draw_list.indices.size();
               }

               auto bindless_infos = gfx::GPUBufferView<GuiBindlessInfo>(pass.Resolve(bindless_info_handle));

               const auto& render_target = pass.Resolve(target);
               cmd.SetViewPort({
                   .x      = draw_data->display_pos.x,
                   .y      = draw_data->display_pos.y,
                   .width  = draw_data->display_size.x,
                   .height = draw_data->display_size.y,
               });
               cmd.SetPipeline(pass.Resolve(pipeline_handle));
               cmd.SetVertexBuffers(0, {{vertex_buffer}}, {{0}});
               cmd.SetIndexBuffer(index_buffer);

               std::size_t draw_call_index = 0;
               vertex_offset               = 0;
               index_offset                = 0;
               for (const auto& draw_list : draw_data->draw_lists) {
                   for (const auto& draw_command : draw_list.commands) {
                       const auto clip_min_x = std::clamp(draw_command.clip_rect.x - draw_data->display_pos.x, 0.0f, draw_data->display_size.x);
                       const auto clip_min_y = std::clamp(draw_command.clip_rect.y - draw_data->display_pos.y, 0.0f, draw_data->display_size.y);
                       const auto clip_max_x = std::clamp(draw_command.clip_rect.z - draw_data->display_pos.x, 0.0f, draw_data->display_size.x);
                       const auto clip_max_y = std::clamp(draw_command.clip_rect.w - draw_data->display_pos.y, 0.0f, draw_data->display_size.y);

                       if (draw_command.element_count == 0 || clip_min_x >= clip_max_x || clip_min_y >= clip_max_y) {
                           continue;
                       }

                       // ImGui clip rectangles are in display space. Clamp them to the active framebuffer
                       // before converting to integer scissors, otherwise negative/minimized rectangles can
                       // underflow Vulkan/DX12 unsigned scissor coordinates.
                       cmd.SetScissorRect({
                           .x      = static_cast<std::uint32_t>(clip_min_x),
                           .y      = static_cast<std::uint32_t>(clip_min_y),
                           .width  = std::min(static_cast<std::uint32_t>(clip_max_x - clip_min_x), render_target.GetDesc().width),
                           .height = std::min(static_cast<std::uint32_t>(clip_max_y - clip_min_y), render_target.GetDesc().height),
                       });

                       bindless_infos[draw_call_index].frame_constant = frame_constant_bindless;
                       bindless_infos[draw_call_index].sampler        = sampler_bindless;
                       bindless_infos[draw_call_index].texture        = draw_command.texture.type == gui::GuiTextureRef::Type::Font
                                                                            ? pass.GetBindless(font_texture_handle)
                                                                            : pass.GetBindless(draw_command.texture.texture);

                       cmd.PushBindlessMetaInfo(gfx::BindlessMetaInfo{
                           .handle = pass.GetBindless(bindless_info_handle, draw_call_index),
                       });

                       cmd.DrawIndexed(
                           draw_command.element_count,
                           1,
                           static_cast<std::uint32_t>(index_offset + draw_command.index_offset),
                           static_cast<std::uint32_t>(vertex_offset + draw_command.vertex_offset));
                       ++draw_call_index;
                   }
                   vertex_offset += draw_list.vertices.size();
                   index_offset += draw_list.indices.size();
               }
           })
        .Finish();
}

}  // namespace hitagi::render
