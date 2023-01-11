#include <hitagi/render/utils.hpp>
#include <hitagi/math/transform.hpp>

namespace hitagi::render {
GuiRenderUtils::GuiRenderUtils(gui::GuiManager& gui_manager, gfx::Device& gfx_device) : m_GuiManager(gui_manager) {
    constexpr std::string_view imgui_shader = R"""(
            cbuffer      ImGuiConstants : register(b0) { matrix projection; };
            SamplerState sampler0       : register(s0);
            Texture2D    texture0       : register(t0);         

            #define RSDEF                                                                      \
              "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), "                                \
              "RootConstants(num32BitConstants=16, b0,  visibility=SHADER_VISIBILITY_VERTEX)," \
              "DescriptorTable(                SRV(t0), visibility=SHADER_VISIBILITY_PIXEL),"  \
              "StaticSampler(                      s0,  visibility=SHADER_VISIBILITY_PIXEL)"            

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

            [RootSignature(RSDEF)]
            PS_INPUT VSMain(VS_INPUT input) {
              PS_INPUT output;
              output.pos = mul(projection, float4(input.pos.xy, 0.f, 1.f));
              output.col = input.col;
              output.uv  = input.uv;
              return output;
            }           

            [RootSignature(RSDEF)]
            float4 PSMain(PS_INPUT input) : SV_TARGET {
              float4 out_col = input.col * texture0.Sample(sampler0, input.uv);
              return out_col;
            }
            )""";

    m_GfxData.pipeline = gfx_device.CreateRenderPipeline({
        .name = "imgui",
        .vs   = {
              .name        = "imgui-vs",
              .type        = gfx::Shader::Type::Vertex,
              .entry       = "VSMain",
              .source_code = std::pmr::string(imgui_shader),
        },
        .ps = {
            .name        = "imgui-ps",
            .type        = gfx::Shader::Type::Pixel,
            .entry       = "PSMain",
            .source_code = std::pmr::string(imgui_shader),
        },
        .topology     = gfx::PrimitiveTopology::TriangleList,
        .input_layout = {
            // clang-format off
            {"POSITION", 0, 0, IM_OFFSETOF(ImDrawVert, pos), gfx::Format::R32G32_FLOAT},
            {"TEXCOORD", 0, 0, IM_OFFSETOF(ImDrawVert,  uv), gfx::Format::R32G32_FLOAT},
            {   "COLOR", 0, 0, IM_OFFSETOF(ImDrawVert, col), gfx::Format::R8G8B8A8_UNORM},
            // clang-format on
        },
        .rasterizer_config = {
            .fill_mode               = gfx::FillMode::Solid,
            .cull_mode               = gfx::CullMode::None,
            .front_counter_clockwise = false,
            .depth_bias              = 0,
            .depth_bias_clamp        = 0.0f,
            .slope_scaled_depth_bias = 0.0f,
            .depth_clip_enable       = true,
            .multisample_enable      = false,
            .antialiased_line_enable = false,
            .forced_sample_count     = 0,
            .conservative_raster     = false,
        },

        .blend_config = {
            .alpha_to_coverage_enable = false,
            .enable_blend             = true,
            .src_blend                = gfx::Blend::SrcAlpha,
            .dest_blend               = gfx::Blend::InvSrcAlpha,
            .blend_op                 = gfx::BlendOp::Add,
            .src_blend_alpha          = gfx::Blend::One,
            .dest_blend_alpha         = gfx::Blend::InvSrcAlpha,
            .blend_op_alpha           = gfx::BlendOp::Add,
        },
        .render_format = gfx::Format::R8G8B8A8_UNORM,
    });

    auto& io = ImGui::GetIO();

    unsigned char* pixels = nullptr;
    int            width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    std::size_t size = width * height * gfx::get_format_bit_size(gfx::Format::R8G8B8A8_UNORM) >> 3;

    m_GfxData.font_texture = gfx_device.CreateTexture(
        {
            .name   = "imgui-font",
            .width  = static_cast<std::uint32_t>(width),
            .height = static_cast<std::uint32_t>(height),
            .format = gfx::Format::R8G8B8A8_UNORM,
        },
        {reinterpret_cast<const std::byte*>(pixels), size});
}

auto GuiRenderUtils::GuiPass(gfx::RenderGraph& render_graph, gfx::ResourceHandle target) -> gfx::ResourceHandle {
    struct GuiRenderPass {
        gfx::ResourceHandle vertices_buffer;
        gfx::ResourceHandle indices_buffer;
        gfx::ResourceHandle output;
    } gui_pass;

    auto font_tex_handle = m_GuiManager.ReadTexture(render_graph.Import("imgui-font", m_GfxData.font_texture));
    ImGui::GetIO().Fonts->SetTexID((void*)(font_tex_handle.id));

    m_GuiManager.Render();

    auto draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr) return target;

    if (m_GfxData.vertices_buffer == nullptr || m_GfxData.vertices_buffer->desc.element_count < draw_data->TotalVtxCount) {
        m_GfxData.vertices_buffer = render_graph.device.CreateBuffer({
            .name          = "imgui-vertices",
            .element_size  = sizeof(ImDrawVert),
            .element_count = static_cast<std::uint64_t>(std::max(1, draw_data->TotalVtxCount)),
            .usages        = gfx::GpuBuffer::UsageFlags::Vertex | gfx::GpuBuffer::UsageFlags::CopyDst,
        });
    }

    if (m_GfxData.indices_buffer == nullptr || m_GfxData.indices_buffer->desc.element_count < draw_data->TotalIdxCount) {
        m_GfxData.indices_buffer = render_graph.device.CreateBuffer({
            .name          = "imgui-indices",
            .element_size  = sizeof(ImDrawIdx),
            .element_count = static_cast<std::uint64_t>(std::max(1, draw_data->TotalIdxCount)),
            .usages        = gfx::GpuBuffer::UsageFlags::Index | gfx::GpuBuffer::UsageFlags::CopyDst,
        });
    }

    std::size_t total_upload_size = draw_data->TotalVtxCount * sizeof(ImDrawVert) + draw_data->TotalIdxCount * sizeof(ImDrawIdx);
    if (m_GfxData.upload_heap == nullptr || m_GfxData.upload_heap->desc.element_size < total_upload_size) {
        m_GfxData.upload_heap = render_graph.device.CreateBuffer({
            .name         = "imgui-upload-heap",
            .element_size = std::max(1ull, total_upload_size),
            .usages       = gfx::GpuBuffer::UsageFlags::MapWrite | gfx::GpuBuffer::UsageFlags::CopySrc,
        });
    }

    auto vertices_buffer = render_graph.Import(m_GfxData.vertices_buffer->desc.name, m_GfxData.vertices_buffer);
    auto indices_buffer  = render_graph.Import(m_GfxData.indices_buffer->desc.name, m_GfxData.indices_buffer);
    auto upload_heap     = render_graph.Import(m_GfxData.upload_heap->desc.name, m_GfxData.upload_heap);

    struct GuiVertexCopyPass {
        gfx::ResourceHandle upload_heap;
        gfx::ResourceHandle vertices_buffer;
        gfx::ResourceHandle indices_buffer;
    };

    auto gui_vertex_copy_pass = render_graph.AddPass<GuiVertexCopyPass>(
        "gui_copy_pass",
        [&](gfx::RenderGraph::Builder& builder, GuiVertexCopyPass& data) {
            data.vertices_buffer = builder.Write(vertices_buffer);
            data.indices_buffer  = builder.Write(indices_buffer);
            data.upload_heap     = builder.Write(upload_heap);
        },
        [=](const gfx::RenderGraph::ResourceHelper& helper, const GuiVertexCopyPass& data, gfx::CopyCommandContext* context) {
            auto& upload_heap = helper.Get<gfx::GpuBuffer>(data.upload_heap);

            std::byte* curr_pointer = upload_heap.mapped_ptr;
            for (std::size_t i = 0; i < draw_data->CmdListsCount; i++) {
                const auto cmd_list = draw_data->CmdLists[i];

                std::memcpy(
                    curr_pointer,
                    cmd_list->VtxBuffer.Data,
                    cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));

                curr_pointer += cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
            }
            std::size_t vertex_total_size = curr_pointer - upload_heap.mapped_ptr;

            for (std::size_t i = 0; i < draw_data->CmdListsCount; i++) {
                const auto cmd_list = draw_data->CmdLists[i];
                std::memcpy(
                    curr_pointer,
                    cmd_list->IdxBuffer.Data,
                    cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
                curr_pointer += cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
            }
            std::size_t index_total_size = (curr_pointer - upload_heap.mapped_ptr) - vertex_total_size;

            auto& vetices_buffer = helper.Get<gfx::GpuBuffer>(data.vertices_buffer);
            auto& indices_buffer = helper.Get<gfx::GpuBuffer>(data.indices_buffer);

            context->CopyBuffer(upload_heap, 0, vetices_buffer, 0, vertex_total_size);
            context->CopyBuffer(upload_heap, vertex_total_size, indices_buffer, 0, index_total_size);
        });

    gui_pass = render_graph.AddPass<GuiRenderPass>(
        "GuiRenderPass",
        [&](gfx::RenderGraph::Builder& builder, GuiRenderPass& data) {
            data.vertices_buffer = builder.Read(gui_vertex_copy_pass.vertices_buffer);
            data.indices_buffer  = builder.Read(gui_vertex_copy_pass.indices_buffer);
            data.output          = builder.Write(target);

            auto textures = m_GuiManager.PopReadTextures();
            for (const auto& tex : textures) {
                builder.Read(tex);
            }
        },
        [=](const gfx::RenderGraph::ResourceHelper& helper, const GuiRenderPass& data, gfx::GraphicsCommandContext* context) {
            const float left   = draw_data->DisplayPos.x;
            const float right  = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
            const float top    = draw_data->DisplayPos.y;
            const float bottom = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
            const float near   = 3.0f;
            const float far    = -1.0f;

            auto& vertices_buffer = helper.Get<gfx::GpuBuffer>(data.vertices_buffer);
            auto& indices_buffer  = helper.Get<gfx::GpuBuffer>(data.indices_buffer);

            context->SetPipeline(*m_GfxData.pipeline);
            context->SetViewPort({
                left,
                top,
                draw_data->DisplaySize.x,
                draw_data->DisplaySize.y,
            });
            context->SetRenderTarget(helper.Get<gfx::Texture>(data.output));
            context->PushConstant(0, math::ortho(left, right, bottom, top, near, far));

            std::size_t vertex_offset = 0;
            std::size_t index_offset  = 0;
            for (std::size_t i = 0; i < draw_data->CmdListsCount; i++) {
                const auto cmd_list = draw_data->CmdLists[i];

                for (const auto& cmd : cmd_list->CmdBuffer) {
                    if (cmd.UserCallback != nullptr) {
                        cmd.UserCallback(cmd_list, &cmd);
                    }
                    if (cmd.ElemCount == 0) continue;

                    math::vec2f clip_min{cmd.ClipRect.x - draw_data->DisplayPos.x, cmd.ClipRect.y - draw_data->DisplayPos.y};
                    math::vec2f clip_max{cmd.ClipRect.z - draw_data->DisplayPos.x, cmd.ClipRect.w - draw_data->DisplayPos.y};
                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                        continue;

                    context->SetScissorRect({
                        .x      = static_cast<std::uint32_t>(clip_min.x),
                        .y      = static_cast<std::uint32_t>(clip_min.y),
                        .width  = static_cast<std::uint32_t>(clip_max.x - clip_min.x),
                        .height = static_cast<std::uint32_t>(clip_max.y - clip_min.y),
                    });

                    if (cmd.TextureId) {
                        context->BindTexture(0, helper.Get<gfx::Texture>({(std::uint64_t)cmd.TextureId}));
                    }

                    context->SetVertexBuffer(0, vertices_buffer);
                    context->SetIndexBuffer(indices_buffer);
                    context->DrawIndexed(cmd.ElemCount, 1, index_offset + cmd.IdxOffset, vertex_offset + cmd.VtxOffset);
                }

                vertex_offset += cmd_list->VtxBuffer.Size;
                index_offset += cmd_list->IdxBuffer.Size;
            }
        });

    return gui_pass.output;
}

}  // namespace hitagi::render