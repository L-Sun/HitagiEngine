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
std::unique_ptr<gui::GuiManager> g_GuiManager = std::make_unique<gui::GuiManager>();
}

namespace hitagi::gui {

int GuiManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("GuiManager");
    m_Logger->info("Initialize...");
    m_Clock.Start();

    m_ImGuiMaterial = g_AssetManager->ImportMaterial("assets/material/imgui.json");

    ImGui::CreateContext();
    if (g_App) {
        ImGui::GetStyle().ScaleAllSizes(g_App->GetDpiRatio());
        ImGui::GetIO().SetPlatformImeDataFn = [](ImGuiViewport* viewport, ImGuiPlatformImeData* data) -> void {
            if (data->WantVisible && g_App)
                g_App->SetInputScreenPosition(data->InputPos.x, data->InputPos.y);
        };
    }

    ;
    m_ImGuiMaterial->SetTexture("font", LoadFontTexture());

    m_Vertices = std::make_shared<VertexArray>(0);
    m_Indices  = std::make_shared<IndexArray>(0, IndexType::UINT16);

    return 0;
}

void GuiManager::Tick() {
    {
        auto& io = ImGui::GetIO();

        // Update window size info.
        auto rect        = g_App->GetWindowsRect();
        io.DisplaySize.x = rect.right - rect.left;
        io.DisplaySize.y = rect.bottom - rect.top;

        // Update HID
        MouseEvent();
        KeysEvent();

        // TODO IME
        for (const auto character : g_InputManager->GetInputText()) {
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

    g_GraphicsManager->AppendRenderables(PrepareImGuiRenderables());

    m_Clock.Tick();
}

void GuiManager::Finalize() {
    ImGui::DestroyContext();
    m_FontsData.clear();
    m_ImGuiMaterial = nullptr;
    m_Vertices      = nullptr;
    m_Indices       = nullptr;
    m_Logger->info("Finalize.");
    m_Logger = nullptr;
}

std::shared_ptr<Texture> GuiManager::LoadFontTexture() {
    auto& io                = ImGui::GetIO();
    io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();

    /* for (const auto& font_file : std::filesystem::directory_iterator{"./Assets/Fonts"}) */ {
        ImFontConfig config;
        config.SizePixels           = (g_App ? g_App->GetDpiRatio() : 1.0f) * 18.0f;
        config.FontDataOwnedByAtlas = false;  // the font data is owned by our engin.

        m_FontsData.emplace_back(g_FileIoManager->SyncOpenAndReadBinary("./assets/fonts/Hasklig-Regular.otf"));
        config.FontData     = m_FontsData.back().GetData();
        config.FontDataSize = m_FontsData.back().GetDataSize();

        std::u8string name = u8"Hasklig-Regular";
        std::copy_n(name.data(), std::min(name.size(), std::size(config.Name)), config.Name);
        io.Fonts->AddFont(&config);

        config.MergeMode = true;

        m_FontsData.emplace_back(g_FileIoManager->SyncOpenAndReadBinary("./assets/fonts/NotoSansSC-Regular.otf"));
        config.FontData     = m_FontsData.back().GetData();
        config.FontDataSize = m_FontsData.back().GetDataSize();
        config.GlyphRanges  = io.Fonts->GetGlyphRangesChineseFull();
        name                = u8"NotoSansSC-Regular";
        std::copy_n(name.data(), std::min(name.size(), std::size(config.Name)), config.Name);
        io.Fonts->AddFont(&config);

        m_FontsData.emplace_back(g_FileIoManager->SyncOpenAndReadBinary("./assets/fonts/NotoSansJP-Regular.otf"));
        config.FontData     = m_FontsData.back().GetData();
        config.FontDataSize = m_FontsData.back().GetDataSize();
        config.GlyphRanges  = io.Fonts->GetGlyphRangesJapanese();
        name                = u8"NotoSansJP-Regular";
        std::copy_n(name.data(), std::min(name.size(), std::size(config.Name)), config.Name);
        io.Fonts->AddFont(&config);
    }

    unsigned char* pixels = nullptr;
    int            width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    const uint32_t bitcount       = 32;
    const size_t   pitch          = width * bitcount / 8;
    auto           texture_buffer = std::make_shared<Image>(width, height, 32, pitch, pitch * height);

    auto p_texture = texture_buffer->Buffer().GetData();
    std::copy_n(reinterpret_cast<const std::byte*>(pixels), texture_buffer->Buffer().GetDataSize(), p_texture);
    return std::make_shared<Texture>(texture_buffer);
}

void GuiManager::MouseEvent() {
    auto& io = ImGui::GetIO();

    io.AddMousePosEvent(g_InputManager->GetFloat(MouseEvent::MOVE_X), g_InputManager->GetFloat(MouseEvent::MOVE_Y));
    io.AddMouseWheelEvent(g_InputManager->GetFloatDelta(MouseEvent::SCROLL_X), g_InputManager->GetFloatDelta(MouseEvent::SCROLL_Y));
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, g_InputManager->GetBool(VirtualKeyCode::MOUSE_L_BUTTON));
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, g_InputManager->GetBool(VirtualKeyCode::MOUSE_R_BUTTON));
    io.AddMouseButtonEvent(ImGuiMouseButton_Middle, g_InputManager->GetBool(VirtualKeyCode::MOUSE_M_BUTTON));
}

void GuiManager::KeysEvent() {
    auto& io = ImGui::GetIO();

    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++) {
        io.AddKeyEvent(key, g_InputManager->GetBool(convert_imgui_key(key)));
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

    m_ImGuiMaterial->SetParameter("orth_projection", projection);

    if (draw_data->TotalIdxCount > m_Indices->IndexCount()) {
        m_Indices->Resize(draw_data->TotalIdxCount);
    }

    if (draw_data->TotalVtxCount > m_Vertices->VertexCount()) {
        m_Vertices->Resize(draw_data->TotalIdxCount);
    }

    auto position  = m_Vertices->GetVertices<VertexAttribute::Position>();
    auto color     = m_Vertices->GetVertices<VertexAttribute::Color0>();
    auto tex_coord = m_Vertices->GetVertices<VertexAttribute::UV0>();
    auto indices   = m_Indices->GetIndices<IndexType::UINT16>();

    std::size_t vertex_offset = 0;
    std::size_t index_offset  = 0;
    for (std::size_t i = 0; i < draw_data->CmdListsCount; i++) {
        const auto cmd_list = draw_data->CmdLists[i];
        for (const auto& vertex : cmd_list->VtxBuffer) {
            auto _color = ImColor(vertex.col).Value;

            position[vertex_offset]  = {vertex.pos.x, vertex.pos.y, 0};
            color[vertex_offset]     = {_color.x, _color.y, _color.z, _color.w};
            tex_coord[vertex_offset] = {vertex.uv.x, vertex.uv.y};

            vertex_offset++;
        }

        std::copy(cmd_list->IdxBuffer.begin(), cmd_list->IdxBuffer.end(), indices.begin() + index_offset);
        index_offset += cmd_list->IdxBuffer.size();

        for (const auto& cmd : cmd_list->CmdBuffer) {
            vec2f clip_min(cmd.ClipRect.x - draw_data->DisplayPos.x, cmd.ClipRect.y - draw_data->DisplayPos.y);
            vec2f clip_max(cmd.ClipRect.z - draw_data->DisplayPos.x, cmd.ClipRect.w - draw_data->DisplayPos.y);
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                continue;

            vec4u scissor_rect = {
                static_cast<uint32_t>(clip_min.x),
                static_cast<uint32_t>(clip_min.y),
                static_cast<uint32_t>(clip_max.x),
                static_cast<uint32_t>(clip_max.y),
            };

            Renderable item;
            item.type      = Renderable::Type::UI;
            item.mesh      = Mesh(m_Vertices, m_Indices, m_ImGuiMaterial, cmd.VtxOffset, cmd.IdxOffset, cmd.ElemCount);
            item.material  = m_ImGuiMaterial->GetMaterial().lock();
            item.transform = std::make_shared<Transform>();

            item.pipeline_parameters.scissor_react = scissor_rect;

            result.emplace_back(std::move(item));
        }
    }

    m_Vertices->IncreaseVersion();
    m_Indices->IncreaseVersion();

    return result;
}

}  // namespace hitagi::gui
