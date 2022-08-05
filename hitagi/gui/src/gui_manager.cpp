#include "imgui_keymaps.hpp"

#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/resource/texture.hpp>
#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/graphics/graphics_manager.hpp>
#include <hitagi/hid/input_manager.hpp>
#include <hitagi/application.hpp>

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

    auto material = asset_manager->ImportMaterial("assets/material/imgui.json");
    if (!material.has_value()) {
        m_Logger->error("Failed to Initialize Gui Manager! Because it gets a empty gui material.");
        return false;
    }
    m_ImGuiMaterialInstance = material.value();

    ImGui::CreateContext();
    if (app) {
        ImGui::GetStyle().ScaleAllSizes(app->GetDpiRatio());
        ImGui::GetIO().SetPlatformImeDataFn = [](ImGuiViewport* viewport, ImGuiPlatformImeData* data) -> void {
            if (data->WantVisible && app)
                app->SetInputScreenPosition(data->InputPos.x, data->InputPos.y);
        };
    }

    m_ImGuiMaterialInstance.SetTexture("imgui-font", LoadFontTexture());

    m_ImGuiMesh.vertices       = std::make_shared<VertexArray>(1);
    m_ImGuiMesh.indices        = std::make_shared<IndexArray>(1, IndexType::UINT16);
    m_ImGuiMesh.vertices->name = "imgui-vertices";
    m_ImGuiMesh.indices->name  = "imgui-indices";

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

    graphics_manager->AppendRenderables(PrepareImGuiRenderables());

    m_Clock.Tick();
}

void GuiManager::Finalize() {
    ImGui::DestroyContext();
    m_ImGuiMesh             = {};
    m_ImGuiMaterialInstance = {};
    m_GuiDrawTasks          = {};
    m_Logger->info("Finalize.");
    m_Logger = nullptr;
}

std::shared_ptr<Texture> GuiManager::LoadFontTexture() {
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
    auto                texture  = std::make_shared<Texture>();
    texture->name                = "imgui-font";
    texture->format              = Format::R8G8B8A8_UNORM;
    texture->width               = width;
    texture->height              = height;
    texture->pitch               = pitch;
    texture->cpu_buffer          = core::Buffer(pitch * height);

    std::copy_n(reinterpret_cast<const std::byte*>(pixels), texture->cpu_buffer.GetDataSize(), texture->cpu_buffer.GetData());

    return texture;
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

std::pmr::vector<Renderable> GuiManager::PrepareImGuiRenderables() {
    auto draw_data = ImGui::GetDrawData();

    std::pmr::vector<Renderable> result;

    const float left       = draw_data->DisplayPos.x;
    const float right      = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    const float top        = draw_data->DisplayPos.y;
    const float bottom     = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    const float near       = 3.0f;
    const float far        = -1.0f;
    const mat4f projection = ortho(left, right, bottom, top, near, far);

    Renderable item{
        .type                = Renderable::Type::UI,
        .vertices            = m_ImGuiMesh.vertices.get(),
        .indices             = m_ImGuiMesh.indices.get(),
        .material            = m_ImGuiMaterialInstance.GetMaterial().lock().get(),
        .material_instance   = &m_ImGuiMaterialInstance,
        .pipeline_parameters = {.view_port = vec4u{
                                    static_cast<std::uint32_t>(draw_data->DisplayPos.x),
                                    static_cast<std::uint32_t>(draw_data->DisplayPos.y),
                                    static_cast<std::uint32_t>(draw_data->DisplaySize.x),
                                    static_cast<std::uint32_t>(draw_data->DisplaySize.y),
                                }},
    };

    m_ImGuiMaterialInstance.SetParameter("orth_projection", projection);

    if (draw_data->TotalVtxCount > m_ImGuiMesh.vertices->vertex_count) {
        m_ImGuiMesh.vertices->Resize(draw_data->TotalVtxCount);
    }

    if (draw_data->TotalIdxCount > m_ImGuiMesh.indices->index_count) {
        m_ImGuiMesh.indices->Resize(draw_data->TotalIdxCount);
    }

    m_ImGuiMesh.sub_meshes.clear();
    std::size_t vertex_offset = 0;
    std::size_t index_offset  = 0;
    for (std::size_t i = 0; i < draw_data->CmdListsCount; i++) {
        const auto cmd_list = draw_data->CmdLists[i];

        for (const auto& cmd : cmd_list->CmdBuffer) {
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

            item.pipeline_parameters.scissor_react = scissor_rect;

            item.index_count   = cmd.ElemCount;
            item.vertex_offset = cmd.VtxOffset + vertex_offset;
            item.index_offset  = cmd.IdxOffset + index_offset;

            result.emplace_back(item);
        }

        for (const auto& vertex : cmd_list->VtxBuffer) {
            auto _color = ImColor(vertex.col).Value;

            m_ImGuiMesh.vertices->Modify<VertexAttribute::Position>([&](auto positions) {
                positions[vertex_offset] = {vertex.pos.x, vertex.pos.y, 0};
            });
            m_ImGuiMesh.vertices->Modify<VertexAttribute::Color0>([&](auto colors) {
                colors[vertex_offset] = {_color.x, _color.y, _color.z, _color.w};
            });
            m_ImGuiMesh.vertices->Modify<VertexAttribute::UV0>([&](auto tex_coords) {
                tex_coords[vertex_offset] = {vertex.uv.x, vertex.uv.y};
            });

            vertex_offset++;
        }
        m_ImGuiMesh.indices->Modify<IndexType::UINT16>([&](auto array) {
            std::copy(cmd_list->IdxBuffer.begin(), cmd_list->IdxBuffer.end(), array.begin() + index_offset);
        });
        index_offset += cmd_list->IdxBuffer.size();
    }

    return result;
}

}  // namespace hitagi::gui
