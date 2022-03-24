#include "GuiManager.hpp"
#include "ImGuiKeyMaps.hpp"
#include "Application.hpp"
#include "InputManager.hpp"
#include "FileIOManager.hpp"

#include <imgui_freetype.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi {
std::unique_ptr<gui::GuiManager> g_GuiManager = std::make_unique<gui::GuiManager>();
}

namespace hitagi::gui {

int GuiManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("GuiManager");
    m_Logger->info("Initialize...");
    m_Clock.Start();

    ImGui::CreateContext();
    ImGui::GetStyle().ScaleAllSizes(g_App->GetDpiRatio());
    ImGui::GetIO().SetPlatformImeDataFn = [](ImGuiViewport* viewport, ImGuiPlatformImeData* data) -> void {
        if (data->WantVisible)
            g_App->SetInputScreenPosition(data->InputPos.x, data->InputPos.y);
    };

    LoadFontTexture();

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

    m_Clock.Tick();
}

void GuiManager::Finalize() {
    ImGui::DestroyContext();
    m_FontsData.clear();
    m_FontTexture = nullptr;
    m_Logger->info("Finalize.");
    m_Logger = nullptr;
}

void GuiManager::LoadFontTexture() {
    auto& io                = ImGui::GetIO();
    io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();

    /* for (const auto& font_file : std::filesystem::directory_iterator{"./Assets/Fonts"}) */ {
        ImFontConfig config;
        config.SizePixels           = g_App->GetDpiRatio() * 18.0f;
        config.FontDataOwnedByAtlas = false;  // the font data is owned by our engin.

        m_FontsData.emplace_back(g_FileIoManager->SyncOpenAndReadBinary("./Assets/Fonts/Hasklig-Regular.otf"));
        config.FontData     = m_FontsData.back().GetData();
        config.FontDataSize = m_FontsData.back().GetDataSize();

        std::u8string name = u8"Hasklig-Regular";
        std::copy_n(name.data(), std::min(name.size(), std::size(config.Name)), config.Name);
        io.Fonts->AddFont(&config);

        config.MergeMode = true;

        m_FontsData.emplace_back(g_FileIoManager->SyncOpenAndReadBinary("./Assets/Fonts/NotoSansSC-Regular.otf"));
        config.FontData     = m_FontsData.back().GetData();
        config.FontDataSize = m_FontsData.back().GetDataSize();
        config.GlyphRanges  = io.Fonts->GetGlyphRangesChineseFull();
        name                = u8"NotoSansSC-Regular";
        std::copy_n(name.data(), std::min(name.size(), std::size(config.Name)), config.Name);
        io.Fonts->AddFont(&config);

        m_FontsData.emplace_back(g_FileIoManager->SyncOpenAndReadBinary("./Assets/Fonts/NotoSansJP-Regular.otf"));
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

    const uint32_t bitcount = 32;
    const size_t   pitch    = width * bitcount / 8;
    m_FontTexture           = std::make_shared<asset::Image>(width, height, 32, pitch, pitch * height);
    uint8_t* p_texture      = m_FontTexture->GetData();
    std::copy_n(reinterpret_cast<const uint8_t*>(pixels), m_FontTexture->GetDataSize(), p_texture);
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

}  // namespace hitagi::gui
