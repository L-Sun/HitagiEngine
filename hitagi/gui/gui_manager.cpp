module;

#include <imgui.h>
#include <imgui_freetype.h>
#include <spdlog/logger.h>

#undef near
#undef far

module gui;
import std;
namespace hitagi::gui {

GuiManager::GuiManager(Application& app) : RuntimeModule("GuiManager"), m_App(app), m_InputManager(app.GetInputManager()) {
    m_Clock.Start();

    ImGui::CreateContext();
    ImGui::GetStyle().ScaleAllSizes(app.GetDpiRatio());

    auto& io                         = ImGui::GetIO();
    auto& platform_io                = ImGui::GetPlatformIO();
    platform_io.Platform_ImeUserData = this;

    platform_io.Platform_SetImeDataFn = [](ImGuiContext*, ImGuiViewport*, ImGuiPlatformImeData* data) -> void {
        if (data->WantVisible) {
            auto p_this = reinterpret_cast<GuiManager*>(ImGui::GetPlatformIO().Platform_ImeUserData);
            p_this->m_App.SetInputScreenPosition({static_cast<unsigned>(data->InputPos.x), static_cast<unsigned>(data->InputPos.y)});
        }
    };
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_DpiEnableScaleFonts;
    io.ConfigWindowsResizeFromEdges = true;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos;

    ImGuiViewport* main_viewport  = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = main_viewport->PlatformHandleRaw = app.GetWindow().ptr;

    LoadFont();
}

GuiManager::~GuiManager() {
    ImGui::DestroyContext();
}

void GuiManager::Tick() {
    auto& io = ImGui::GetIO();

    // Update window size info.
    auto rect        = m_App.GetWindowRect();
    io.DisplaySize.x = rect.right - rect.left;
    io.DisplaySize.y = rect.bottom - rect.top;

    // Update HID
    MouseEvent();
    KeysEvent();

    // TODO IME
    for (const auto character : m_InputManager.GetInputText()) {
        io.AddInputCharacter(character);
    }

    // Update delta time
    io.DeltaTime = m_Clock.DeltaTime().count();

    ImGui::NewFrame();
    while (!m_GuiDrawTasks.empty()) {
        m_GuiDrawTasks.front()();
        m_GuiDrawTasks.pop();
    }
    ImGui::Render();
    BuildDrawData();

    m_Clock.Tick();
}

void GuiManager::LoadFont() {
    auto& io = ImGui::GetIO();

    /* for (const auto& font_file : std::filesystem::directory_iterator{"./Assets/Fonts"}) */ {
        ImFontConfig config;
        config.SizePixels           = m_App.GetDpiRatio() * 18.0f;
        config.FontDataOwnedByAtlas = false;  // the font data is owned by our engin.

        if (core::FileIOManager::Get()) {
            {
                auto& font_buffer   = core::FileIOManager::Get()->SyncOpenAndReadBinary("./assets/fonts/Hasklig-Regular.otf");
                config.FontData     = const_cast<std::byte*>(font_buffer.GetData());
                config.FontDataSize = font_buffer.GetDataSize();

                std::pmr::u8string name = u8"Hasklig-Regular";
                std::copy_n(name.data(), std::min(name.size(), std::size(config.Name)), config.Name);
                io.Fonts->AddFont(&config);
            }

            config.MergeMode = true;

            {
                auto& font_buffer       = core::FileIOManager::Get()->SyncOpenAndReadBinary("./assets/fonts/NotoSansSC-Regular.otf");
                config.FontData         = const_cast<std::byte*>(font_buffer.GetData());
                config.FontDataSize     = font_buffer.GetDataSize();
                config.GlyphRanges      = io.Fonts->GetGlyphRangesChineseFull();
                std::pmr::u8string name = u8"NotoSansSC-Regular";
                std::copy_n(name.data(), std::min(name.size(), std::size(config.Name)), config.Name);
                io.Fonts->AddFont(&config);
            }

            {
                auto& font_buffer       = core::FileIOManager::Get()->SyncOpenAndReadBinary("./assets/fonts/NotoSansJP-Regular.otf");
                config.FontData         = const_cast<std::byte*>(font_buffer.GetData());
                config.FontDataSize     = font_buffer.GetDataSize();
                config.GlyphRanges      = io.Fonts->GetGlyphRangesJapanese();
                std::pmr::u8string name = u8"NotoSansJP-Regular";
                std::copy_n(name.data(), std::min(name.size(), std::size(config.Name)), config.Name);
                io.Fonts->AddFont(&config);
            }
        }
    }

    unsigned char* pixels = nullptr;
    int            width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    const auto data_size = static_cast<std::size_t>(width) *
                           static_cast<std::size_t>(height) *
                           gfx::get_format_byte_size(gfx::Format::R8G8B8A8_UNORM);

    m_FontAtlasWidth  = static_cast<std::uint32_t>(width);
    m_FontAtlasHeight = static_cast<std::uint32_t>(height);
    m_FontAtlasPixels.assign(reinterpret_cast<const std::byte*>(pixels), reinterpret_cast<const std::byte*>(pixels) + data_size);
    ++m_FontAtlasGeneration;

    io.Fonts->TexID = (ImTextureID)0;
}

void GuiManager::BuildDrawData() {
    m_DrawData.display_pos  = {};
    m_DrawData.display_size = {};
    m_DrawData.font_atlas   = {
        .width      = m_FontAtlasWidth,
        .height     = m_FontAtlasHeight,
        .pixels     = m_FontAtlasPixels,
        .generation = m_FontAtlasGeneration,
    };
    m_DrawData.draw_lists.clear();

    const auto draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr || draw_data->CmdListsCount == 0) {
        return;
    }

    m_DrawData.display_pos  = {draw_data->DisplayPos.x, draw_data->DisplayPos.y};
    m_DrawData.display_size = {draw_data->DisplaySize.x, draw_data->DisplaySize.y};
    m_DrawData.draw_lists.reserve(static_cast<std::size_t>(draw_data->CmdListsCount));

    for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index) {
        const auto im_draw_list = draw_data->CmdLists[list_index];
        auto&      draw_list    = m_DrawData.draw_lists.emplace_back();

        draw_list.vertices.reserve(static_cast<std::size_t>(im_draw_list->VtxBuffer.Size));
        for (const auto& vertex : im_draw_list->VtxBuffer) {
            draw_list.vertices.emplace_back(GuiVertex{
                .position = {vertex.pos.x, vertex.pos.y},
                .uv       = {vertex.uv.x, vertex.uv.y},
                .color    = DecodeColor(vertex.col),
            });
        }

        draw_list.indices.reserve(static_cast<std::size_t>(im_draw_list->IdxBuffer.Size));
        for (const auto index : im_draw_list->IdxBuffer) {
            draw_list.indices.emplace_back(static_cast<std::uint32_t>(index));
        }

        draw_list.commands.reserve(static_cast<std::size_t>(im_draw_list->CmdBuffer.Size));
        for (const auto& command : im_draw_list->CmdBuffer) {
            if (command.UserCallback != nullptr) {
                // The renderer now consumes backend-neutral draw packets. ImGui render callbacks are backend
                // escape hatches, so they cannot safely be replayed after conversion.
                continue;
            }

            draw_list.commands.emplace_back(GuiDrawCommand{
                .element_count = command.ElemCount,
                .index_offset  = command.IdxOffset,
                .vertex_offset = command.VtxOffset,
                .clip_rect     = {command.ClipRect.x, command.ClipRect.y, command.ClipRect.z, command.ClipRect.w},
                .texture       = DecodeTexture(command.GetTexID()),
            });
        }
    }
}

void GuiManager::MouseEvent() {
    auto& io = ImGui::GetIO();

    if (!(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)) {
        if (io.MouseDrawCursor) {
            m_App.SetCursor(Cursor::None);
        } else {
            ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
            switch (imgui_cursor) {
                case ImGuiMouseCursor_None:

                case ImGuiMouseCursor_Arrow:
                    m_App.SetCursor(Cursor::Arrow);
                    break;
                case ImGuiMouseCursor_TextInput:
                    m_App.SetCursor(Cursor::TextInput);
                    break;
                case ImGuiMouseCursor_ResizeAll:
                    m_App.SetCursor(Cursor::ResizeAll);
                    break;
                case ImGuiMouseCursor_ResizeEW:
                    m_App.SetCursor(Cursor::ResizeEW);
                    break;
                case ImGuiMouseCursor_ResizeNS:
                    m_App.SetCursor(Cursor::ResizeNS);
                    break;
                case ImGuiMouseCursor_ResizeNESW:
                    m_App.SetCursor(Cursor::ResizeNESW);
                    break;
                case ImGuiMouseCursor_ResizeNWSE:
                    m_App.SetCursor(Cursor::ResizeNWSE);
                    break;
                case ImGuiMouseCursor_Hand:
                    m_App.SetCursor(Cursor::Hand);
                    break;
                case ImGuiMouseCursor_NotAllowed:
                    m_App.SetCursor(Cursor::Forbid);
                    break;
            }
        }
    }

    io.AddMousePosEvent(m_InputManager.GetFloat(hid::MouseEvent::MOVE_X), m_InputManager.GetFloat(hid::MouseEvent::MOVE_Y));
    io.AddMouseWheelEvent(m_InputManager.GetFloatDelta(hid::MouseEvent::SCROLL_X), m_InputManager.GetFloatDelta(hid::MouseEvent::SCROLL_Y));
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, m_InputManager.GetBool(hid::VirtualKeyCode::MOUSE_L_BUTTON));
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, m_InputManager.GetBool(hid::VirtualKeyCode::MOUSE_R_BUTTON));
    io.AddMouseButtonEvent(ImGuiMouseButton_Middle, m_InputManager.GetBool(hid::VirtualKeyCode::MOUSE_M_BUTTON));
}

void GuiManager::KeysEvent() {
    auto& io = ImGui::GetIO();

    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++) {
        io.AddKeyEvent(static_cast<ImGuiKey>(key), m_InputManager.GetBool(convert_imgui_key(static_cast<ImGuiKey>(key))));
    }
}

auto GuiManager::ReadTexture(rg::TextureHandle texture) -> ImTextureID {
    if (!texture) {
        return (ImTextureID)0;
    }
    return (ImTextureID)(texture.index + 1);
}

auto GuiManager::DecodeColor(std::uint32_t color) noexcept -> math::Color {
    constexpr auto inv_255 = 1.0f / 255.0f;
    return {
        static_cast<float>((color >> IM_COL32_R_SHIFT) & 0xFF) * inv_255,
        static_cast<float>((color >> IM_COL32_G_SHIFT) & 0xFF) * inv_255,
        static_cast<float>((color >> IM_COL32_B_SHIFT) & 0xFF) * inv_255,
        static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) * inv_255,
    };
}

auto GuiManager::DecodeTexture(ImTextureID texture_id) noexcept -> GuiTextureRef {
    const auto id = static_cast<std::size_t>(texture_id);
    if (id == 0) {
        return {.type = GuiTextureRef::Type::Font};
    }
    return {
        .type    = GuiTextureRef::Type::RenderGraph,
        .texture = rg::TextureHandle{id - 1},
    };
}

}  // namespace hitagi::gui
