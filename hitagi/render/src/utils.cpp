#include <hitagi/render/utils.hpp>
#include <hitagi/math/transform.hpp>

#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/transform.hpp>
#include "hitagi/gfx/render_graph.hpp"

namespace hitagi::render {
GuiRenderUtils::GuiRenderUtils(gui::GuiManager& gui_manager, gfx::Device& gfx_device) : m_GuiManager(gui_manager) {
    constexpr std::string_view imgui_shader = R"""(
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
            .usages = gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopyDst,
        },
        {reinterpret_cast<const std::byte*>(pixels), size});
}

auto GuiRenderUtils::GuiPass(gfx::RenderGraph& render_graph, gfx::ResourceHandle target) -> gfx::ResourceHandle {
    struct GuiRenderPass {
        gfx::ResourceHandle bindless_infos;
        gfx::ResourceHandle frame_constant;
        gfx::ResourceHandle vertices_buffer;
        gfx::ResourceHandle indices_buffer;
        gfx::ResourceHandle sampler;
        gfx::ResourceHandle pipeline;
        gfx::ResourceHandle output;
    } gui_pass;

    struct FrameConstant {
        math::mat4f orth;
    };
    struct BindlessInfo {
        gfx::BindlessHandle frame_constant;
        gfx::BindlessHandle texture;
        gfx::BindlessHandle sampler;
    };

    auto font_tex_handle = m_GuiManager.ReadTexture(render_graph.Import(m_GfxData.font_texture));
    ImGui::GetIO().Fonts->SetTexID((void*)(font_tex_handle.node_index));

    m_GuiManager.Render();

    auto draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr || draw_data->CmdListsCount == 0) return target;

    if (m_GfxData.vertices_buffer == nullptr || m_GfxData.vertices_buffer->GetDesc().element_count < static_cast<std::uint64_t>(draw_data->TotalVtxCount)) {
        m_GfxData.vertices_buffer = render_graph.GetDevice().CreateGPUBuffer({
            .name          = "imgui-vertices",
            .element_size  = sizeof(ImDrawVert),
            .element_count = static_cast<std::uint64_t>(std::max(1, draw_data->TotalVtxCount)),
            .usages        = gfx::GPUBufferUsageFlags::Vertex | gfx::GPUBufferUsageFlags::MapWrite,
        });
    }

    if (m_GfxData.indices_buffer == nullptr || m_GfxData.indices_buffer->GetDesc().element_count < static_cast<std::uint64_t>(draw_data->TotalIdxCount)) {
        m_GfxData.indices_buffer = render_graph.GetDevice().CreateGPUBuffer({
            .name          = "imgui-indices",
            .element_size  = sizeof(ImDrawIdx),
            .element_count = static_cast<std::uint64_t>(std::max(1, draw_data->TotalIdxCount)),
            .usages        = gfx::GPUBufferUsageFlags::Index | gfx::GPUBufferUsageFlags::MapWrite,
        });
    }

    if (m_GfxData.sampler == nullptr) {
        m_GfxData.sampler = render_graph.GetDevice().CreateSampler({
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
    }

    if (m_GfxData.pipeline == nullptr) {
        const auto& target_info = render_graph.GetResourceInfo(target);
        gfx::Format render_format;
        if (target_info.resource) {
            if (target_info.resource->GetType() == gfx::ResourceType::SwapChain) {
                render_format = std::dynamic_pointer_cast<gfx::SwapChain>(target_info.resource)->GetFormat();
            } else {
                render_format = std::dynamic_pointer_cast<gfx::Texture>(target_info.resource)->GetDesc().format;
            }
        } else {
            render_format = std::get<gfx::TextureDesc>(target_info.desc).format;
        }

        m_GfxData.pipeline = render_graph.GetDevice().CreateRenderPipeline({
            .name           = "imgui",
            .shaders        = {m_GfxData.vs, m_GfxData.ps},
            .assembly_state = {
                .primitive = gfx::PrimitiveTopology::TriangleList,
            },
            .vertex_input_layout = {
                // clang-format off
            {"POSITION", 0, gfx::Format::R32G32_FLOAT, 0, IM_OFFSETOF(ImDrawVert, pos),sizeof(ImDrawVert)},
            {"TEXCOORD", 0, gfx::Format::R32G32_FLOAT, 0, IM_OFFSETOF(ImDrawVert,  uv),sizeof(ImDrawVert)},
            {   "COLOR", 0, gfx::Format::R8G8B8A8_UNORM, 0, IM_OFFSETOF(ImDrawVert, col),sizeof(ImDrawVert)},
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
            .render_format = render_format,
        });
    }

    std::size_t num_draw_calls = ranges::accumulate(
        std::span(draw_data->CmdLists, draw_data->CmdListsCount) |
            ranges::views::transform([](auto cmd_list) { return cmd_list->CmdBuffer.size(); }),
        0);

    gui_pass = render_graph.AddPass<GuiRenderPass, gfx::CommandType::Graphics>(
        "GuiRenderPass",
        [&](auto& builder, GuiRenderPass& data) {
            data.bindless_infos  = builder.Read(render_graph.Create(gfx::GPUBufferDesc{
                 .name          = "imgui_bindless_info",
                 .element_size  = sizeof(BindlessInfo),
                 .element_count = num_draw_calls,
                 .usages        = gfx::GPUBufferUsageFlags::Constant | gfx::GPUBufferUsageFlags::MapWrite,
            }));
            data.frame_constant  = builder.Read(render_graph.Create(gfx::GPUBufferDesc{
                 .name         = "imgui_frame_constant",
                 .element_size = sizeof(FrameConstant),
                 .usages       = gfx::GPUBufferUsageFlags::Constant | gfx::GPUBufferUsageFlags::MapWrite,
            }));
            data.vertices_buffer = builder.Read(render_graph.Import(m_GfxData.vertices_buffer));
            data.indices_buffer  = builder.Read(render_graph.Import(m_GfxData.indices_buffer));
            data.sampler         = builder.Read(render_graph.Import(m_GfxData.sampler));
            data.pipeline        = builder.Read(render_graph.Import(m_GfxData.pipeline));
            data.output          = builder.Write(target);

            auto textures = m_GuiManager.PopReadTextures();
            for (const auto& tex : textures) {
                builder.Read(tex);
            }
        },
        [=](const gfx::ResourceHelper& helper, const GuiRenderPass& data, gfx::GraphicsCommandContext& context) {
            const float left   = draw_data->DisplayPos.x;
            const float right  = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
            const float top    = draw_data->DisplayPos.y;
            const float bottom = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
            const float near   = 3.0f;
            const float far    = -1.0f;

            auto bindless_infos = gfx::GPUBufferView<BindlessInfo>(helper.Get<gfx::GPUBuffer>(data.bindless_infos));

            auto frame_constant         = gfx::GPUBufferView<FrameConstant>(helper.Get<gfx::GPUBuffer>(data.frame_constant));
            frame_constant.front().orth = math::ortho(left, right, bottom, top, near, far);

            auto& vertices_buffer = helper.Get<gfx::GPUBuffer>(data.vertices_buffer);
            auto& indices_buffer  = helper.Get<gfx::GPUBuffer>(data.indices_buffer);
            // Copy vertex and index
            {
                auto vertices = gfx::GPUBufferView<ImDrawVert>(vertices_buffer);
                auto indices  = gfx::GPUBufferView<ImDrawIdx>(indices_buffer);
                for (int cmd_index = 0, curr_vertex_offset = 0, curr_index_offset = 0; cmd_index < draw_data->CmdListsCount; cmd_index++) {
                    const auto cmd_list = draw_data->CmdLists[cmd_index];

                    std::copy(cmd_list->VtxBuffer.begin(), cmd_list->VtxBuffer.end(), std::next(vertices.begin(), curr_vertex_offset));
                    std::copy(cmd_list->IdxBuffer.begin(), cmd_list->IdxBuffer.end(), std::next(indices.begin(), curr_index_offset));

                    curr_vertex_offset += cmd_list->VtxBuffer.size();
                    curr_index_offset += cmd_list->IdxBuffer.size();
                }
            }

            context.SetViewPort({
                left,
                top,
                draw_data->DisplaySize.x,
                draw_data->DisplaySize.y,
            });
            context.SetPipeline(helper.Get<gfx::RenderPipeline>(data.pipeline));

            context.BeginRendering(helper.Get<gfx::Texture>(data.output));
            for (int i = 0, draw_call_index = 0, vertex_offset = 0, index_offset = 0; i < draw_data->CmdListsCount; i++) {
                const auto cmd_list = draw_data->CmdLists[i];

                for (const auto& cmd : cmd_list->CmdBuffer) {
                    if (cmd.UserCallback != nullptr) {
                        cmd.UserCallback(cmd_list, &cmd);
                    }

                    math::vec2f clip_min{cmd.ClipRect.x - draw_data->DisplayPos.x, cmd.ClipRect.y - draw_data->DisplayPos.y};
                    math::vec2f clip_max{cmd.ClipRect.z - draw_data->DisplayPos.x, cmd.ClipRect.w - draw_data->DisplayPos.y};

                    if (cmd.ElemCount != 0 && clip_min.x < clip_max.x && clip_min.y < clip_max.y) {
                        context.SetScissorRect({
                            .x      = static_cast<std::uint32_t>(clip_min.x),
                            .y      = static_cast<std::uint32_t>(clip_min.y),
                            .width  = static_cast<std::uint32_t>(clip_max.x - clip_min.x),
                            .height = static_cast<std::uint32_t>(clip_max.y - clip_min.y),
                        });

                        bindless_infos[draw_call_index].frame_constant = helper.GetCBV(data.frame_constant);
                        bindless_infos[draw_call_index].texture        = cmd.TextureId ? helper.GetSRV({(std::uint64_t)cmd.TextureId}) : gfx::BindlessHandle{};
                        bindless_infos[draw_call_index].sampler        = helper.GetSampler(data.sampler);

                        // TODO
                        context.PushBindlessMetaInfo(gfx::BindlessMetaInfo{
                            .handle = helper.GetCBV(data.bindless_infos, draw_call_index),
                        });

                        context.SetVertexBuffer(0, vertices_buffer);
                        context.SetIndexBuffer(indices_buffer);
                        context.DrawIndexed(cmd.ElemCount, 1, index_offset + cmd.IdxOffset, vertex_offset + cmd.VtxOffset);
                    }

                    draw_call_index++;
                }

                vertex_offset += cmd_list->VtxBuffer.Size;
                index_offset += cmd_list->IdxBuffer.Size;
            }
            context.EndRendering();
        });

    return gui_pass.output;
}

auto SwapChainRenderingLayoutTransitionPass(gfx::RenderGraph& render_graph, gfx::ResourceHandle swap_chain) -> gfx::ResourceHandle {
    auto device_type = render_graph.GetDevice().device_type;

    return render_graph.AddPass<gfx::ResourceHandle, gfx::CommandType::Graphics>(
        "SwapChainRenderingLayoutTransitionPass",
        [&](auto& builder, gfx::ResourceHandle& output) {
            output = builder.Write(swap_chain);
        },
        [=](const gfx::ResourceHelper& helper, const gfx::ResourceHandle& output, gfx::GraphicsCommandContext& context) {
            gfx::TextureBarrier barrier{
                .src_layout = gfx::TextureLayout::Unkown,
                .dst_layout = gfx::TextureLayout::RenderTarget,
                .texture    = helper.Get<gfx::Texture>(output),
            };

            if (device_type == gfx::Device::Type::DX12) {
                barrier.src_access = gfx::BarrierAccess::None;
                barrier.dst_access = gfx::BarrierAccess::None;
                barrier.src_stage  = gfx::PipelineStage::None;
                barrier.dst_stage  = gfx::PipelineStage::None;
            } else if (device_type == gfx::Device::Type::Vulkan) {
                barrier.src_access = gfx::BarrierAccess::None;
                barrier.dst_access = gfx::BarrierAccess::RenderTarget;
                barrier.src_stage  = gfx::PipelineStage::Render;
                barrier.dst_stage  = gfx::PipelineStage::Render;
            }

            context.ResourceBarrier({}, {}, {barrier});
        });
}

}  // namespace hitagi::render
