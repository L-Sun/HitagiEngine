#include <hitagi/render/utils.hpp>
#include <hitagi/gfx/utils.hpp>
#include <hitagi/math/transform.hpp>

#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

namespace hitagi::render {
GuiRenderUtils::GuiRenderUtils(gui::GuiManager& gui_manager, gfx::Device& gfx_device) : m_GuiManager(gui_manager) {
    const std::pmr::string imgui_shader = R"""(
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
            float3 pos : POSITION;
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
        .name        = "imgui-vs",
        .type        = gfx::ShaderType::Vertex,
        .entry       = "VSMain",
        .source_code = imgui_shader,
    });

    m_GfxData.ps = gfx_device.CreateShader({
        .name        = "imgui-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSMain",
        .source_code = imgui_shader,
    });

    m_GfxData.sampler = gfx_device.CreateSampler({
        .name           = "imgui-sampler",
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
        .name           = "imgui",
        .shaders        = {m_GfxData.vs, m_GfxData.ps},
        .assembly_state = {
            .primitive = gfx::PrimitiveTopology::TriangleList,
        },
        .vertex_input_layout = {
            // clang-format off
            {"POSITION", gfx::Format::R32G32_FLOAT, 0, IM_OFFSETOF(ImDrawVert, pos),sizeof(ImDrawVert)},
            {"TEXCOORD", gfx::Format::R32G32_FLOAT, 0, IM_OFFSETOF(ImDrawVert,  uv),sizeof(ImDrawVert)},
            {   "COLOR", gfx::Format::R8G8B8A8_UNORM, 0, IM_OFFSETOF(ImDrawVert, col),sizeof(ImDrawVert)},
            // clang-format on
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

    auto& io = ImGui::GetIO();

    unsigned char* pixels = nullptr;
    int            width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    std::size_t size = width * height * gfx::get_format_byte_size(gfx::Format::R8G8B8A8_UNORM);

    m_GfxData.font_texture = gfx_device.CreateTexture(
        {
            .name   = "imgui-font",
            .width  = static_cast<std::uint32_t>(width),
            .height = static_cast<std::uint32_t>(height),
            .format = gfx::Format::R8G8B8A8_UNORM,
            .usages = gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopyDst,
        },
        {reinterpret_cast<const std::byte*>(pixels), size});

    ImGui::GetIO().Fonts->TexID = reinterpret_cast<ImTextureID>(&m_FontTexture);
}

void GuiRenderUtils::GuiPass(rg::RenderGraph& render_graph, rg::TextureHandle target, bool clear_target) {
    if (m_GfxData.font_texture == nullptr) return;

    struct FrameConstant {
        math::mat4f orth;
    };
    struct BindlessInfo {
        gfx::BindlessHandle frame_constant;
        gfx::BindlessHandle texture;
        gfx::BindlessHandle sampler;
    };

    m_FontTexture = render_graph.Import(m_GfxData.font_texture, "imgui_font");
    m_GuiManager.ReadTexture(m_FontTexture);

    auto draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr || draw_data->CmdListsCount == 0) return;

    std::size_t num_draw_calls = ranges::accumulate(
        std::span(draw_data->CmdLists, draw_data->CmdListsCount) |
            ranges::views::transform([](auto cmd_list) { return cmd_list->CmdBuffer.size(); }),
        0);

    rg::RenderPassBuilder builder(render_graph);
    builder.SetName("GuiRenderPass")
        .Read(render_graph.Create(
            {
                .name          = "imgui_bindless_info",
                .element_size  = sizeof(BindlessInfo),
                .element_count = num_draw_calls,
                .usages        = gfx::GPUBufferUsageFlags::Constant | gfx::GPUBufferUsageFlags::MapWrite,
            },
            "imgui_bindless_info"))
        .Read(render_graph.Create(
            {
                .name         = "imgui_frame_constant",
                .element_size = sizeof(FrameConstant),
                .usages       = gfx::GPUBufferUsageFlags::Constant | gfx::GPUBufferUsageFlags::MapWrite,
            },
            "imgui_frame_constant"))
        .ReadAsVertices(render_graph.Create(
            gfx::GPUBufferDesc{
                .element_size  = sizeof(ImDrawVert),
                .element_count = static_cast<std::uint64_t>(draw_data->TotalVtxCount),
                .usages        = gfx::GPUBufferUsageFlags::Vertex | gfx::GPUBufferUsageFlags::MapWrite,
            },
            "imgui_vertices"))
        .ReadAsIndices(render_graph.Create(
            gfx::GPUBufferDesc{
                .element_size  = sizeof(ImDrawIdx),
                .element_count = static_cast<std::uint64_t>(draw_data->TotalIdxCount),
                .usages        = gfx::GPUBufferUsageFlags::Index | gfx::GPUBufferUsageFlags::MapWrite,
            },
            "imgui_indices"))
        .AddSampler(render_graph.Import(m_GfxData.sampler, "imgui_sampler"))
        .AddPipeline(render_graph.Import(m_GfxData.pipeline, "imgui_pipeline"))
        .SetRenderTarget(target, clear_target);

    for (const auto texture_handle : m_GuiManager.PopReadTextures()) {
        builder.Read(texture_handle, {}, gfx::PipelineStage::PixelShader);
    }

    builder.SetExecutor([=, font_texture = m_FontTexture](const rg::RenderGraph& render_graph, const rg::RenderPassNode& pass) {
               auto draw_data = ImGui::GetDrawData();

               const auto vertex_buffer_handle = render_graph.GetBufferHandle("imgui_vertices");
               auto&      vertex_buffer        = pass.Resolve(vertex_buffer_handle);
               auto       vertex_buffer_view   = gfx::GPUBufferView<ImDrawVert>(vertex_buffer);

               const auto index_buffer_handle = render_graph.GetBufferHandle("imgui_indices");
               auto&      index_buffer        = pass.Resolve(index_buffer_handle);
               auto       index_buffer_view   = gfx::GPUBufferView<ImDrawIdx>(index_buffer);

               const auto frame_constant_handle   = render_graph.GetBufferHandle("imgui_frame_constant");
               const auto frame_constant_bindless = pass.GetBindless(frame_constant_handle);
               {
                   gfx::GPUBufferView<FrameConstant> frame_constant(pass.Resolve(frame_constant_handle));
                   frame_constant.front().orth = math::ortho(
                       draw_data->DisplayPos.x,
                       draw_data->DisplayPos.x + draw_data->DisplaySize.x,
                       draw_data->DisplayPos.y + draw_data->DisplaySize.y,
                       draw_data->DisplayPos.y,
                       3.0f,
                       -1.0f);
               }
               const auto sampler_bindless = pass.GetBindless(render_graph.GetSamplerHandle("imgui_sampler"));

               const auto bindless_infos_handle = render_graph.GetBufferHandle("imgui_bindless_info");
               auto       bindless_infos        = gfx::GPUBufferView<BindlessInfo>(pass.Resolve(bindless_infos_handle));

               auto& cmd = pass.GetCmd();
               cmd.SetViewPort({
                   draw_data->DisplayPos.x,
                   draw_data->DisplayPos.y,
                   draw_data->DisplaySize.x,
                   draw_data->DisplaySize.y,
               });
               cmd.SetPipeline(pass.Resolve(render_graph.GetRenderPipelineHandle("imgui_pipeline")));
               cmd.SetVertexBuffers(0, {{vertex_buffer}}, {{0}});
               cmd.SetIndexBuffer(index_buffer);

               for (int i = 0, draw_call_index = 0, vertex_offset = 0, index_offset = 0; i < draw_data->CmdListsCount; i++) {
                   const auto im_draw_list = draw_data->CmdLists[i];

                   std::memcpy(vertex_buffer_view.data() + vertex_offset, im_draw_list->VtxBuffer.Data, im_draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
                   std::memcpy(index_buffer_view.data() + index_offset, im_draw_list->IdxBuffer.Data, im_draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));

                   for (const auto& im_cmd : im_draw_list->CmdBuffer) {
                       if (im_cmd.UserCallback != nullptr) {
                           im_cmd.UserCallback(im_draw_list, &im_cmd);
                       }

                       const math::vec2f clip_min{im_cmd.ClipRect.x - draw_data->DisplayPos.x, im_cmd.ClipRect.y - draw_data->DisplayPos.y};
                       const math::vec2f clip_max{im_cmd.ClipRect.z - draw_data->DisplayPos.x, im_cmd.ClipRect.w - draw_data->DisplayPos.y};

                       if (im_cmd.ElemCount == 0 || clip_min.x >= clip_max.x || clip_min.y >= clip_max.y)
                           continue;

                       if (clip_min.x < 0 && clip_min.y < 0) {
                           continue;
                       }
                       cmd.SetScissorRect({
                           .x      = static_cast<std::uint32_t>(clip_min.x),
                           .y      = static_cast<std::uint32_t>(clip_min.y),
                           .width  = static_cast<std::uint32_t>(clip_max.x - clip_min.x),
                           .height = static_cast<std::uint32_t>(clip_max.y - clip_min.y),
                       });

                       bindless_infos[draw_call_index].frame_constant = frame_constant_bindless;
                       bindless_infos[draw_call_index].sampler        = sampler_bindless;

                       if (im_cmd.TextureId == ImGui::GetIO().Fonts->TexID) {
                           bindless_infos[draw_call_index].texture = pass.GetBindless(font_texture);
                       } else {
                           bindless_infos[draw_call_index].texture = pass.GetBindless(rg::TextureHandle((std::size_t)(im_cmd.TextureId)));
                       }

                       cmd.PushBindlessMetaInfo(gfx::BindlessMetaInfo{
                           .handle = pass.GetBindless(bindless_infos_handle, draw_call_index),
                       });

                       cmd.DrawIndexed(im_cmd.ElemCount, 1, index_offset + im_cmd.IdxOffset, vertex_offset + im_cmd.VtxOffset);
                       draw_call_index++;
                   }
                   vertex_offset += im_draw_list->VtxBuffer.Size;
                   index_offset += im_draw_list->IdxBuffer.Size;
               }
           })
        .Finish();
}

}  // namespace hitagi::render
