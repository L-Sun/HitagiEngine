#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/math/transform.hpp>
#include <hitagi/hid/input_manager.hpp>
#include <hitagi/application.hpp>

#include "imgui_keymaps.hpp"

#include <imgui.h>
#include <imgui_freetype.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#undef near
#undef far

namespace hitagi {
gui::GuiManager* gui_manager = nullptr;
}

namespace hitagi::gui {

bool GuiManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("GuiManager");
    m_Logger->info("Initialize...");
    m_Clock.Start();

    ImGui::CreateContext();
    if (app) {
        ImGui::GetStyle().ScaleAllSizes(app->GetDpiRatio());
        ImGui::GetIO().SetPlatformImeDataFn = [](ImGuiViewport* viewport, ImGuiPlatformImeData* data) -> void {
            if (data->WantVisible && app)
                app->SetInputScreenPosition(data->InputPos.x, data->InputPos.y);
        };
    }
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_DpiEnableScaleFonts;

    LoadFont();

    return true;
}

void GuiManager::Tick() {
    auto& io = ImGui::GetIO();

    // Update window size info.
    auto rect        = app->GetWindowsRect();
    io.DisplaySize.x = rect.right - rect.left;
    io.DisplaySize.y = rect.bottom - rect.top;

    // Update HID
    MouseEvent();
    KeysEvent();

    // TODO IME
    for (const auto character : input_manager->GetInputText()) {
        io.AddInputCharacter(character);
    }

    // Update delta time
    io.DeltaTime = m_Clock.DeltaTime().count();

    m_ReadTextures.clear();

    m_Clock.Tick();
}

void GuiManager::Finalize() {
    ImGui::DestroyContext();

    m_GuiDrawTasks = {};
    m_GfxData      = {};

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void GuiManager::LoadFont() {
    auto& io                = ImGui::GetIO();
    io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();

    /* for (const auto& font_file : std::filesystem::directory_iterator{"./Assets/Fonts"}) */ {
        ImFontConfig config;
        config.SizePixels           = (app ? app->GetDpiRatio() : 1.0f) * 18.0f;
        config.FontDataOwnedByAtlas = false;  // the font data is owned by our engin.

        {
            auto& font_buffer   = file_io_manager->SyncOpenAndReadBinary("./assets/fonts/Hasklig-Regular.otf");
            config.FontData     = const_cast<std::byte*>(font_buffer.GetData());
            config.FontDataSize = font_buffer.GetDataSize();

            std::pmr::u8string name = u8"Hasklig-Regular";
            std::copy_n(name.data(), std::min(name.size(), std::size(config.Name)), config.Name);
            io.Fonts->AddFont(&config);
        }

        config.MergeMode = true;

        {
            auto& font_buffer       = file_io_manager->SyncOpenAndReadBinary("./assets/fonts/NotoSansSC-Regular.otf");
            config.FontData         = const_cast<std::byte*>(font_buffer.GetData());
            config.FontDataSize     = font_buffer.GetDataSize();
            config.GlyphRanges      = io.Fonts->GetGlyphRangesChineseFull();
            std::pmr::u8string name = u8"NotoSansSC-Regular";
            std::copy_n(name.data(), std::min(name.size(), std::size(config.Name)), config.Name);
            io.Fonts->AddFont(&config);
        }

        {
            auto& font_buffer       = file_io_manager->SyncOpenAndReadBinary("./assets/fonts/NotoSansJP-Regular.otf");
            config.FontData         = const_cast<std::byte*>(font_buffer.GetData());
            config.FontDataSize     = font_buffer.GetDataSize();
            config.GlyphRanges      = io.Fonts->GetGlyphRangesJapanese();
            std::pmr::u8string name = u8"NotoSansJP-Regular";
            std::copy_n(name.data(), std::min(name.size(), std::size(config.Name)), config.Name);
            io.Fonts->AddFont(&config);
        }
    }
}

void GuiManager::InitRenderPipeline(gfx::Device& gfx_device) {
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

        .vs = gfx_device.CompileShader({
            .name        = "imgui-vs",
            .type        = gfx::Shader::Type::Vertex,
            .entry       = "VSMain",
            .source_code = imgui_shader,
        }),
        .ps = gfx_device.CompileShader({
            .name        = "imgui-ps",
            .type        = gfx::Shader::Type::Pixel,
            .entry       = "PSMain",
            .source_code = imgui_shader,
        }),

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
}

void GuiManager::InitFontTexture(gfx::Device& gfx_device) {
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

void GuiManager::MouseEvent() {
    auto& io = ImGui::GetIO();

    io.AddMousePosEvent(input_manager->GetFloat(MouseEvent::MOVE_X), input_manager->GetFloat(MouseEvent::MOVE_Y));
    io.AddMouseWheelEvent(input_manager->GetFloatDelta(MouseEvent::SCROLL_X), input_manager->GetFloatDelta(MouseEvent::SCROLL_Y));
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, input_manager->GetBool(VirtualKeyCode::MOUSE_L_BUTTON));
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, input_manager->GetBool(VirtualKeyCode::MOUSE_R_BUTTON));
    io.AddMouseButtonEvent(ImGuiMouseButton_Middle, input_manager->GetBool(VirtualKeyCode::MOUSE_M_BUTTON));
}

void GuiManager::KeysEvent() {
    auto& io = ImGui::GetIO();

    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++) {
        io.AddKeyEvent(key, input_manager->GetBool(convert_imgui_key(key)));
    }
}

auto GuiManager::ReadTexture(gfx::ResourceHandle tex) -> gfx::ResourceHandle {
    return m_ReadTextures.emplace_back(tex);
}

auto GuiManager::GuiRenderPass(gfx::RenderGraph& rg, gfx::ResourceHandle tex) -> gfx::ResourceHandle {
    if (m_GfxData.pipeline == nullptr) {
        InitRenderPipeline(rg.device);
    }

    if (m_GfxData.font_texture == nullptr) {
        InitFontTexture(rg.device);
    }

    auto& io = ImGui::GetIO();
    io.Fonts->SetTexID((void*)ReadTexture(rg.Import("imgui-font", m_GfxData.font_texture)).id);

    ImGui::NewFrame();
    while (!m_GuiDrawTasks.empty()) {
        m_GuiDrawTasks.front()();
        m_GuiDrawTasks.pop();
    }
    ImGui::Render();
    auto draw_data = ImGui::GetDrawData();

    if (m_GfxData.vertices_buffer == nullptr || m_GfxData.vertices_buffer->desc.size < draw_data->TotalVtxCount * sizeof(ImDrawVert)) {
        m_GfxData.vertices_buffer = rg.device.CreateBuffer({
            .name   = "imgui-vertices",
            .size   = draw_data->TotalVtxCount * sizeof(ImDrawVert),
            .usages = gfx::GpuBuffer::UsageFlags::CopyDst,
        });
    }

    if (m_GfxData.indices_buffer == nullptr || m_GfxData.indices_buffer->desc.size < draw_data->TotalIdxCount * sizeof(ImDrawIdx)) {
        m_GfxData.indices_buffer = rg.device.CreateBuffer({
            .name   = "imgui-indices",
            .size   = draw_data->TotalIdxCount * sizeof(ImDrawIdx),
            .usages = gfx::GpuBuffer::UsageFlags::CopyDst,
        });
    }

    if (m_GfxData.upload_heap == nullptr || m_GfxData.upload_heap->desc.size < m_GfxData.indices_buffer->desc.size + m_GfxData.vertices_buffer->desc.size) {
        m_GfxData.upload_heap = rg.device.CreateBuffer({
            .name   = "imgui-upload-healp",
            .size   = m_GfxData.indices_buffer->desc.size + m_GfxData.vertices_buffer->desc.size,
            .usages = gfx::GpuBuffer::UsageFlags::MapWrite | gfx::GpuBuffer::UsageFlags::CopySrc,
        });
    }

    auto vertices_buffer = rg.Import(m_GfxData.vertices_buffer->desc.name, m_GfxData.vertices_buffer);
    auto indices_buffer  = rg.Import(m_GfxData.indices_buffer->desc.name, m_GfxData.indices_buffer);
    auto upload_heap     = rg.Import(m_GfxData.upload_heap->desc.name, m_GfxData.upload_heap);

    struct GuiVertexCopyPass {
        gfx::ResourceHandle upload_heap;
        gfx::ResourceHandle vertices_buffer;
        gfx::ResourceHandle indices_buffer;
    };
    auto vertex_copy_pass = rg.AddPass<GuiVertexCopyPass>(
        "vertex_copy_pass",
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

    struct GuiRenderPass {
        gfx::ResourceHandle vertices_buffer;
        gfx::ResourceHandle indices_buffer;
        gfx::ResourceHandle output;
    };

    auto gui_pass = rg.AddPass<GuiRenderPass>(
        "GuiRenderPass",
        [&](gfx::RenderGraph::Builder& builder, GuiRenderPass& data) {
            data.vertices_buffer = builder.Read(vertex_copy_pass.vertices_buffer);
            data.indices_buffer  = builder.Read(vertex_copy_pass.indices_buffer);
            data.output          = builder.Write(tex);
            for (const auto& tex : m_ReadTextures) {
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

            auto  rtv             = context->device->CreateTextureView({.textuer = helper.Get<gfx::Texture>(data.output)});
            auto& vertices_buffer = helper.Get<gfx::GpuBuffer>(data.vertices_buffer);
            auto& indices_buffer  = helper.Get<gfx::GpuBuffer>(data.indices_buffer);

            context->SetPipeline(*m_GfxData.pipeline);
            context->SetViewPort({
                left,
                top,
                draw_data->DisplaySize.x,
                draw_data->DisplaySize.y,
            });
            context->SetRenderTarget(*rtv);
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
                        auto tex = context->device->CreateTextureView({
                            .textuer = helper.Get<gfx::Texture>({(std::uint64_t)cmd.TextureId}),
                        });
                        context->BindTexture(0, *tex);
                    }

                    auto vbv = context->device->CreateBufferView({
                        .buffer = vertices_buffer,
                        .offset = vertex_offset * sizeof(ImDrawVert),
                        .stride = sizeof(ImDrawVert),
                        .usages = gfx::GpuBufferView::UsageFlags::Vertex,
                    });
                    auto ibv = context->device->CreateBufferView({
                        .buffer = indices_buffer,
                        .offset = index_offset * sizeof(ImDrawIdx),
                        .stride = sizeof(ImDrawIdx),
                        .usages = gfx::GpuBufferView::UsageFlags::Index,
                    });
                    context->SetVertexBuffer(0, *vbv);
                    context->SetIndexBuffer(*ibv);
                    context->DrawIndexed(cmd.ElemCount, 1, cmd.IdxOffset, cmd.VtxOffset);
                }

                vertex_offset += cmd_list->VtxBuffer.Size;
                index_offset += cmd_list->IdxBuffer.Size;
            }
        });

    return gui_pass.output;
}

}  // namespace hitagi::gui
