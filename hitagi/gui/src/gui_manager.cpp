#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/math/transform.hpp>
#include <hitagi/hid/input_manager.hpp>
#include <hitagi/application.hpp>

#include "imgui_keymaps.hpp"

#include <imgui.h>
#include <imgui_freetype.h>
#include <spdlog/logger.h>

#undef near
#undef far

namespace hitagi::gui {

GuiManager::GuiManager(Application& app) : RuntimeModule("GuiManager"), m_App(app) {
    m_Clock.Start();

    ImGui::CreateContext();
    ImGui::GetStyle().ScaleAllSizes(app.GetDpiRatio());

    auto& io    = ImGui::GetIO();
    io.UserData = this;

    io.SetPlatformImeDataFn = [](ImGuiViewport* viewport, ImGuiPlatformImeData* data) -> void {
        if (data->WantVisible) {
            auto p_this = reinterpret_cast<GuiManager*>(ImGui::GetIO().UserData);
            p_this->m_App.SetInputScreenPosition({static_cast<unsigned>(data->InputPos.x), static_cast<unsigned>(data->InputPos.y)});
        }
    };
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_DpiEnableScaleFonts;
    io.ConfigWindowsResizeFromEdges = true;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos;

    ImGuiViewport* main_viewport  = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = main_viewport->PlatformHandleRaw = app.GetWindow();

    LoadFont();
}

GuiManager::~GuiManager() {
    ImGui::DestroyContext();
}

void GuiManager::Tick() {
    auto& io = ImGui::GetIO();

    // Update window size info.
    auto rect        = m_App.GetWindowsRect();
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

    m_Clock.Tick();
}

void GuiManager::Render() {
    ImGui::NewFrame();
    while (!m_GuiDrawTasks.empty()) {
        m_GuiDrawTasks.front()();
        m_GuiDrawTasks.pop();
    }
    ImGui::Render();
}

void GuiManager::LoadFont() {
    auto& io                = ImGui::GetIO();
    io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();

    /* for (const auto& font_file : std::filesystem::directory_iterator{"./Assets/Fonts"}) */ {
        ImFontConfig config;
        config.SizePixels           = m_App.GetDpiRatio() * 18.0f;
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

}  // namespace hitagi::gui
