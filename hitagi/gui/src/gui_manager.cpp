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

    m_DrawData.mesh = {
        .indices = std::make_shared<IndexArray>(1, "imgui-indices", IndexType::UINT16),
    };
    m_DrawData.mesh.vertices[VertexAttribute::Position] = std::make_shared<VertexArray>(VertexAttribute::Position, 1, "imgui-vertex-pos");
    m_DrawData.mesh.vertices[VertexAttribute::Color0]   = std::make_shared<VertexArray>(VertexAttribute::Color0, 1, "imgui-vertex-pos");
    m_DrawData.mesh.vertices[VertexAttribute::UV0]      = std::make_shared<VertexArray>(VertexAttribute::UV0, 1, "imgui-vertex-pos");
    LoadFontTexture();

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
    ImGui::NewFrame();

    while (!m_GuiDrawTasks.empty()) {
        m_GuiDrawTasks.front()();
        m_GuiDrawTasks.pop();
    }

    ImGui::Render();

    graphics_manager->DrawGui(GetDrawData());
    m_Clock.Tick();
}

void GuiManager::Finalize() {
    ImGui::DestroyContext();
    m_GuiDrawTasks = {};
    m_DrawData     = {};
    m_FontTexture  = nullptr;
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
    io.Fonts->SetTexID(m_FontTexture.get());
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

auto GuiManager::GetDrawData() -> const gfx::GuiDrawData& {
    auto draw_data = ImGui::GetDrawData();

    // std::pmr::vector<Renderable> result;

    const float left     = draw_data->DisplayPos.x;
    const float right    = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    const float top      = draw_data->DisplayPos.y;
    const float bottom   = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    const float near     = 3.0f;
    const float far      = -1.0f;
    m_DrawData.view_port = {
        static_cast<std::uint32_t>(left),
        static_cast<std::uint32_t>(top),
        static_cast<std::uint32_t>(draw_data->DisplaySize.x),
        static_cast<std::uint32_t>(draw_data->DisplaySize.y)};
    m_DrawData.projection = ortho(left, right, bottom, top, near, far);

    for (const auto& attribute_array : m_DrawData.mesh.vertices) {
        if (attribute_array) attribute_array->Resize(draw_data->TotalVtxCount);
    }

    if (draw_data->TotalIdxCount > m_DrawData.mesh.indices->index_count) {
        m_DrawData.mesh.indices->Resize(draw_data->TotalIdxCount);
    }

    m_DrawData.mesh.sub_meshes.clear();
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

            vec4u scissor_rect = {
                static_cast<std::uint32_t>(clip_min.x),
                static_cast<std::uint32_t>(clip_min.y),
                static_cast<std::uint32_t>(clip_max.x),
                static_cast<std::uint32_t>(clip_max.y),
            };
            m_DrawData.mesh.sub_meshes.emplace_back(Mesh::SubMesh{
                .index_count   = cmd.ElemCount,
                .index_offset  = cmd.IdxOffset + index_offset,
                .vertex_offset = cmd.VtxOffset + vertex_offset,
            });
            m_DrawData.scissor_rects.emplace_back(scissor_rect);
            m_DrawData.textures.emplace_back(static_cast<resource::Texture*>(cmd.GetTexID()));
        }

        for (const auto& vertex : cmd_list->VtxBuffer) {
            auto _color = ImColor(vertex.col).Value;

            m_DrawData.mesh.Modify<VertexAttribute::Position>([&](auto positions) {
                positions[vertex_offset] = {vertex.pos.x, vertex.pos.y, 0};
            });
            m_DrawData.mesh.Modify<VertexAttribute::Color0>([&](auto colors) {
                colors[vertex_offset] = {_color.x, _color.y, _color.z, _color.w};
            });
            m_DrawData.mesh.Modify<VertexAttribute::UV0>([&](auto tex_coords) {
                tex_coords[vertex_offset] = {vertex.uv.x, vertex.uv.y};
            });

            vertex_offset++;
        }
        m_DrawData.mesh.Modify<IndexType::UINT16>([&](auto array) {
            std::copy(cmd_list->IdxBuffer.begin(), cmd_list->IdxBuffer.end(), array.begin() + index_offset);
        });
        index_offset += cmd_list->IdxBuffer.size();
    }

    return m_DrawData;
}

}  // namespace hitagi::gui
