#include "imgui_keymaps.hpp"

#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/resource/texture.hpp>
#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/hid/input_manager.hpp>
#include <hitagi/application.hpp>
#include <hitagi/graphics/graphics_manager.hpp>

#include <imgui.h>
#include <imgui_freetype.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#undef near
#undef far

using namespace hitagi::resource;
using namespace hitagi::math;

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

    m_DrawData.mesh_data = {
        .indices = std::make_shared<IndexArray>(1, "imgui-indices", IndexType::UINT16),
    };
    m_DrawData.mesh_data.vertices[VertexAttribute::Position] = std::make_shared<VertexArray>(VertexAttribute::Position, 1, "imgui-vertex-pos");
    m_DrawData.mesh_data.vertices[VertexAttribute::Color0]   = std::make_shared<VertexArray>(VertexAttribute::Color0, 1, "imgui-vertex-pos");
    m_DrawData.mesh_data.vertices[VertexAttribute::UV0]      = std::make_shared<VertexArray>(VertexAttribute::UV0, 1, "imgui-vertex-pos");
    LoadFontTexture();

    InitRenderPipeline();

    return true;
}

void GuiManager::Tick() {
    {
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
    }

    m_Clock.Tick();
}

void GuiManager::Finalize() {
    ImGui::DestroyContext();
    m_GuiDrawTasks = {};
    graphics_manager->GetDevice()->RetireResource(std::move(m_Pipeline.gpu_resource));
    m_Pipeline    = {};
    m_FontTexture = nullptr;
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void GuiManager::LoadFontTexture() {
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

    unsigned char* pixels = nullptr;
    int            width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    const std::uint32_t bitcount = 32;
    const std::size_t   pitch    = width * bitcount / 8;
    m_FontTexture                = std::make_shared<Texture>();
    m_FontTexture->name          = "imgui-font";
    m_FontTexture->format        = Format::R8G8B8A8_UNORM;
    m_FontTexture->width         = width;
    m_FontTexture->height        = height;
    m_FontTexture->pitch         = pitch;
    m_FontTexture->cpu_buffer    = core::Buffer(pitch * height);

    std::copy_n(reinterpret_cast<const std::byte*>(pixels), m_FontTexture->cpu_buffer.GetDataSize(), m_FontTexture->cpu_buffer.GetData());
    graphics_manager->GetDevice()->InitTexture(*m_FontTexture);
}

void GuiManager::InitRenderPipeline() {
    m_Pipeline = {
        .vs             = std::make_shared<resource::Shader>(resource::Shader::Type::Vertex, "assets/shaders/imgui.hlsl"),
        .ps             = std::make_shared<resource::Shader>(resource::Shader::Type::Pixel, "assets/shaders/imgui.hlsl"),
        .primitive_type = resource::PrimitiveType::TriangleList,
        .blend_state    = {
               .alpha_to_coverage_enable = false,
               .enable_blend             = true,
               .src_blend                = gfx::Blend::SrcAlpha,
               .dest_blend               = gfx::Blend::InvSrcAlpha,
               .blend_op                 = gfx::BlendOp::Add,
               .src_blend_alpha          = gfx::Blend::One,
               .dest_blend_alpha         = gfx::Blend::InvSrcAlpha,
               .blend_op_alpha           = gfx::BlendOp::Add,
        },
        .rasterizer_state = {
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
        .render_format = resource::Format::R8G8B8A8_UNORM,
    };
    m_Pipeline.name = "gui";
    graphics_manager->GetDevice()->InitPipelineState(m_Pipeline);
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

void GuiManager::UpdateMeshData() {
    auto draw_data = ImGui::GetDrawData();

    for (const auto& attribute_array : m_DrawData.mesh_data.vertices) {
        if (attribute_array && draw_data->TotalVtxCount > attribute_array->vertex_count) {
            attribute_array->Resize(draw_data->TotalVtxCount);
        }
    }

    if (draw_data->TotalIdxCount > m_DrawData.mesh_data.indices->index_count) {
        m_DrawData.mesh_data.indices->Resize(draw_data->TotalIdxCount);
    }

    m_DrawData.mesh_data.sub_meshes.clear();
    m_DrawData.scissor_rects.clear();
    m_DrawData.textures.clear();

    std::size_t vertex_offset = 0;
    std::size_t index_offset  = 0;
    for (std::size_t i = 0; i < draw_data->CmdListsCount; i++) {
        const auto cmd_list = draw_data->CmdLists[i];
        for (const auto& cmd : cmd_list->CmdBuffer) {
            if (cmd.UserCallback != nullptr) {
                cmd.UserCallback(cmd_list, &cmd);
            }
            if (cmd.ElemCount == 0) continue;

            vec2f clip_min(cmd.ClipRect.x - draw_data->DisplayPos.x, cmd.ClipRect.y - draw_data->DisplayPos.y);
            vec2f clip_max(cmd.ClipRect.z - draw_data->DisplayPos.x, cmd.ClipRect.w - draw_data->DisplayPos.y);
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                continue;

            m_DrawData.mesh_data.sub_meshes.emplace_back(Mesh::SubMesh{
                .index_count       = cmd.ElemCount,
                .index_offset      = cmd.IdxOffset + index_offset,
                .vertex_offset     = cmd.VtxOffset + vertex_offset,
                .primitive         = PrimitiveType::TriangleList,
                .material_instance = nullptr,
            });
            m_DrawData.scissor_rects.emplace_back(
                static_cast<std::uint32_t>(clip_min.x),
                static_cast<std::uint32_t>(clip_min.y),
                static_cast<std::uint32_t>(clip_max.x),
                static_cast<std::uint32_t>(clip_max.y));
            m_DrawData.textures.emplace_back((gfx::ResourceHandle)cmd.TextureId);
        }
        for (const auto& vertex : cmd_list->VtxBuffer) {
            auto _color = ImColor(vertex.col).Value;

            m_DrawData.mesh_data.Modify<VertexAttribute::Position>([&](auto positions) {
                positions[vertex_offset] = {vertex.pos.x, vertex.pos.y, 0};
            });
            m_DrawData.mesh_data.Modify<VertexAttribute::Color0>([&](auto colors) {
                colors[vertex_offset] = {_color.x, _color.y, _color.z, _color.w};
            });
            m_DrawData.mesh_data.Modify<VertexAttribute::UV0>([&](auto tex_coords) {
                tex_coords[vertex_offset] = {vertex.uv.x, vertex.uv.y};
            });

            vertex_offset++;
        }
        m_DrawData.mesh_data.Modify<IndexType::UINT16>([&](auto array) {
            std::copy(cmd_list->IdxBuffer.begin(), cmd_list->IdxBuffer.end(), array.begin() + index_offset);
        });
        index_offset += cmd_list->IdxBuffer.size();
    }
}

auto GuiManager::Render(gfx::RenderGraph* render_graph, gfx::ResourceHandle output) -> GuiRenderPass {
    auto font_texture = graphics_manager->GetRenderGraph()->Import(m_FontTexture.get());

    auto& io = ImGui::GetIO();
    io.Fonts->SetTexID((void*)font_texture);

    ImGui::NewFrame();

    while (!m_GuiDrawTasks.empty()) {
        m_GuiDrawTasks.front()();
        m_GuiDrawTasks.pop();
    }
    ImGui::Render();
    UpdateMeshData();

    auto draw_data = ImGui::GetDrawData();

    const float left   = draw_data->DisplayPos.x;
    const float right  = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    const float top    = draw_data->DisplayPos.y;
    const float bottom = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    const float near   = 3.0f;
    const float far    = -1.0f;

    vec4u view_port = {
        static_cast<std::uint32_t>(left),
        static_cast<std::uint32_t>(top),
        static_cast<std::uint32_t>(draw_data->DisplaySize.x),
        static_cast<std::uint32_t>(draw_data->DisplaySize.y)};
    mat4f projection = ortho(left, right, bottom, top, near, far);

    return render_graph->AddPass<GuiRenderPass>(
        "GuiPass",
        [&](gfx::RenderGraph::Builder& builder, GuiRenderPass& data) {
            for (auto tex : m_DrawData.textures) {
                data.textures.emplace_back(builder.Read(tex));
            }
            data.output = builder.Write(output);
        },
        [=, this](const gfx::RenderGraph::ResourceHelper& helper, const GuiRenderPass& data, gfx::IGraphicsCommandContext* context) {
            if (m_DrawData.mesh_data.sub_meshes.size() == 0) return;

            context->SetPipelineState(m_Pipeline);

            context->SetRenderTarget(helper.Get<resource::Texture>(data.output));
            context->BindDynamicConstantBuffer(0, reinterpret_cast<const std::byte*>(&projection), sizeof(projection));
            context->SetViewPort(view_port.x, view_port.y, view_port.z, view_port.w);
            for (const auto& attribute_array : m_DrawData.mesh_data.vertices) {
                if (attribute_array) context->BindDynamicVertexBuffer(*attribute_array);
            }
            context->BindDynamicIndexBuffer(*m_DrawData.mesh_data.indices);

            const std::size_t num_sub_meshes = m_DrawData.mesh_data.sub_meshes.size();
            for (std::size_t i = 0; i < num_sub_meshes; i++) {
                const auto& sub_mesh = m_DrawData.mesh_data.sub_meshes[i];
                const auto& scissor  = m_DrawData.scissor_rects[i];
                const auto& texture  = helper.Get<Texture>(m_DrawData.textures[i]);

                context->SetScissorRect(scissor.x, scissor.y, scissor.z, scissor.w);
                context->BindResource(0, texture);
                context->DrawIndexed(sub_mesh.index_count, sub_mesh.index_offset, sub_mesh.vertex_offset);
            }
        });
}

}  // namespace hitagi::gui
