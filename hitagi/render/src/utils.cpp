#include <hitagi/render/utils.hpp>
#include <hitagi/math/transform.hpp>

namespace hitagi::render {
GuiRenderUtils::GuiRenderUtils(gui::GuiManager& gui_manager, gfx::Device& gfx_device) : m_GuiManager(gui_manager) {
    constexpr std::string_view imgui_shader = R"""(
            #include "bindless.hlsl"
            struct BindlessInfo {
                SimpleBuffer frame_constant;
                Texture      texture;
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
                BindlessInfo bindless_info = hitagi::load_bindless<BindlessInfo>();
                FrameConstant frame_constant = bindless_info.frame_constant.Load<FrameConstant>(0);

                PS_INPUT output;
                output.pos = mul(frame_constant.projection, float4(input.pos.xy, 0.f, 1.f));
                output.col = input.col;
                output.uv  = input.uv;
                return output;
            }           

            float4 PSMain(PS_INPUT input) : SV_TARGET {
                BindlessInfo bindless_info = hitagi::load_bindless<BindlessInfo>();
                Texture texture = bindless_info.frame_constant.Load<FrameConstant>(0);
                
                float4 out_col = input.col * texture.sample(sampler0, input.uv);
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

    m_GfxData.pipeline = gfx_device.CreateRenderPipeline({
        .name           = "imgui",
        .shaders        = {m_GfxData.vs, m_GfxData.ps},
        .assembly_state = {
            .primitive = gfx::PrimitiveTopology::TriangleList,
        },
        .vertex_input_layout = {
            // clang-format off
            {"POSITION", 0, gfx::Format::R32G32_FLOAT, 0, IM_OFFSETOF(ImDrawVert, pos)},
            {"TEXCOORD", 0, gfx::Format::R32G32_FLOAT, 0, IM_OFFSETOF(ImDrawVert,  uv)},
            {   "COLOR", 0, gfx::Format::R8G8B8A8_UNORM, 0, IM_OFFSETOF(ImDrawVert, col)},
            // clang-format on
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
        gfx::ResourceHandle bindless_info;
        gfx::ResourceHandle frame_constant;
        gfx::ResourceHandle vertices_buffer;
        gfx::ResourceHandle indices_buffer;
        gfx::ResourceHandle output;
    } gui_pass;

    struct FrameConstant {
        math::mat4f orth;
    };
    struct BindlessInfo {
        gfx::BindlessHandle frame_constant;
        gfx::BindlessHandle texture;
    };

    auto font_tex_handle = m_GuiManager.ReadTexture(render_graph.Import("imgui-font", m_GfxData.font_texture));
    ImGui::GetIO().Fonts->SetTexID((void*)(font_tex_handle.id));

    m_GuiManager.Render();

    auto draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr) return target;

    if (m_GfxData.vertices_buffer == nullptr || m_GfxData.vertices_buffer->GetDesc().element_count < draw_data->TotalVtxCount) {
        m_GfxData.vertices_buffer = render_graph.device.CreateGPUBuffer({
            .name          = "imgui-vertices",
            .element_size  = sizeof(ImDrawVert),
            .element_count = static_cast<std::uint64_t>(std::max(1, draw_data->TotalVtxCount)),
            .usages        = gfx::GPUBufferUsageFlags::Vertex | gfx::GPUBufferUsageFlags::MapWrite,
        });
    }

    if (m_GfxData.indices_buffer == nullptr || m_GfxData.indices_buffer->GetDesc().element_count < draw_data->TotalIdxCount) {
        m_GfxData.indices_buffer = render_graph.device.CreateGPUBuffer({
            .name          = "imgui-indices",
            .element_size  = sizeof(ImDrawIdx),
            .element_count = static_cast<std::uint64_t>(std::max(1, draw_data->TotalIdxCount)),
            .usages        = gfx::GPUBufferUsageFlags::Index | gfx::GPUBufferUsageFlags::MapWrite,
        });
    }

    std::byte*  curr_vertices_buffer_pointer = m_GfxData.vertices_buffer->GetMappedPtr();
    std::byte*  curr_indices_buffer_pointer  = m_GfxData.indices_buffer->GetMappedPtr();
    std::size_t num_draw_calls               = 0;
    for (int i = 0; i < draw_data->CmdListsCount; i++) {
        const auto cmd_list = draw_data->CmdLists[i];
        std::memcpy(
            curr_vertices_buffer_pointer,
            cmd_list->VtxBuffer.Data,
            cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));

        std::memcpy(
            curr_indices_buffer_pointer,
            cmd_list->IdxBuffer.Data,
            cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));

        curr_vertices_buffer_pointer += cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
        curr_indices_buffer_pointer += cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
        num_draw_calls += draw_data->CmdLists[i]->CmdBuffer.size();
    }

    auto vertices_buffer = render_graph.Import(m_GfxData.vertices_buffer->GetDesc().name, m_GfxData.vertices_buffer);
    auto indices_buffer  = render_graph.Import(m_GfxData.indices_buffer->GetDesc().name, m_GfxData.indices_buffer);

    gui_pass = render_graph.AddPass<GuiRenderPass>(
        "GuiRenderPass",
        [&](gfx::RenderGraph::Builder& builder, GuiRenderPass& data) {
            builder.UseRenderPipeline(m_GfxData.pipeline);

            data.bindless_info   = builder.Create(gfx::GPUBufferDesc{
                  .name          = "imgui_bindless_info",
                  .element_size  = sizeof(BindlessInfo),
                  .element_count = num_draw_calls,
                  .usages        = gfx::GPUBufferUsageFlags::Constant | gfx::GPUBufferUsageFlags::MapWrite,
            });
            data.frame_constant  = builder.Create(gfx::GPUBufferDesc{
                 .name         = "imgui_frame_constant",
                 .element_size = sizeof(FrameConstant),
                 .usages       = gfx::GPUBufferUsageFlags::Constant | gfx::GPUBufferUsageFlags::MapWrite,
            });
            data.vertices_buffer = builder.Read(vertices_buffer);
            data.indices_buffer  = builder.Read(indices_buffer);
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

            auto& bindless_info = helper.Get<gfx::GPUBuffer>(data.bindless_info);

            auto& frame_constant = helper.Get<gfx::GPUBuffer>(data.frame_constant);
            frame_constant.Update(0, FrameConstant{.orth = math::ortho(left, right, bottom, top, near, far)});

            auto& vertices_buffer = helper.Get<gfx::GPUBuffer>(data.vertices_buffer);
            auto& indices_buffer  = helper.Get<gfx::GPUBuffer>(data.indices_buffer);

            context->SetPipeline(*m_GfxData.pipeline);
            context->SetViewPort({
                left,
                top,
                draw_data->DisplaySize.x,
                draw_data->DisplaySize.y,
            });
            context->BeginRendering({
                .render_target = helper.Get<gfx::Texture>(data.output),
            });

            std::size_t vertex_offset   = 0;
            std::size_t index_offset    = 0;
            std::size_t draw_call_index = 0;
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

                    bindless_info.Update(
                        draw_call_index,
                        BindlessInfo{
                            .frame_constant = helper.GetBindlessHandle(data.frame_constant),
                            .texture        = cmd.TextureId ? helper.GetBindlessHandle({(std::uint64_t)cmd.TextureId}) : gfx::BindlessHandle{},
                        });
                    // TODO
                    // context->PushBindlessMetaInfo(helper.GetBindlessHandle(data.bindless_info, draw_call_index));

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
