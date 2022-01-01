#include "GuiManager.hpp"
#include "Application.hpp"
#include "InputManager.hpp"
#include "FileIOManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Hitagi {
std::unique_ptr<Gui::GuiManager> g_GuiManager = std::make_unique<Gui::GuiManager>();
}

namespace Hitagi::Gui {

int GuiManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("GuiManager");
    m_Logger->info("Initialize...");
    m_Clock.Start();

    ImGui::CreateContext();
    ImGui::GetStyle().ScaleAllSizes(g_App->GetDpiRatio());
    LoadFontTexture();
    MapKey();

    return 0;
}

void GuiManager::Tick() {
    m_Clock.Tick();
    {
        auto& io = ImGui::GetIO();

        // Update window size info.
        auto rect        = g_App->GetWindowsRect();
        io.DisplaySize.x = rect.right - rect.left;
        io.DisplaySize.y = rect.bottom - rect.top;

        // Update HID
        io.MousePos = ImVec2(g_InputManager->GetFloat(MouseEvent::MOVE_X), g_InputManager->GetFloat(MouseEvent::MOVE_Y));
        io.MouseWheel += g_InputManager->GetFloatDelta(MouseEvent::SCROLL);
        io.MouseDown[0] = g_InputManager->GetBool(VirtualKeyCode::MOUSE_L_BUTTON);
        io.MouseDown[1] = g_InputManager->GetBool(VirtualKeyCode::MOUSE_R_BUTTON);
        io.MouseDown[2] = g_InputManager->GetBool(VirtualKeyCode::MOUSE_M_BUTTON);
        for (int key : io.KeyMap) {
            io.KeysDown[key] = g_InputManager->GetBool(static_cast<VirtualKeyCode>(key));
        }
        io.KeyCtrl  = g_InputManager->GetBool(VirtualKeyCode::KEY_CTRL);
        io.KeyShift = g_InputManager->GetBool(VirtualKeyCode::KEY_SHIFT);
        io.KeyAlt   = g_InputManager->GetBool(VirtualKeyCode::KEY_ALT);
        // TODO IME
        io.AddInputCharactersUTF8(reinterpret_cast<const char*>(g_InputManager->GetInputText().c_str()));

        // Update delta time
        io.DeltaTime = m_Clock.DeltaTime().count();
    }
    ImGui::NewFrame();

    while (!m_GuiDrawTasks.empty()) {
        m_GuiDrawTasks.front()();
        m_GuiDrawTasks.pop();
    }

    ImGui::Render();
}

void GuiManager::Finalize() {
    ImGui::DestroyContext();
    m_FontsData.clear();
    m_FontTexture = nullptr;
    m_Logger->info("Finalize.");
    m_Logger = nullptr;
}

void GuiManager::LoadFontTexture() {
    auto& io = ImGui::GetIO();

    /* for (const auto& font_file : std::filesystem::directory_iterator{"./Assets/Fonts"}) */ {
        auto& font_data = m_FontsData.emplace_back(g_FileIoManager->SyncOpenAndReadBinary("./Assets/Fonts/Hasklig-Regular.otf"));

        ImFontConfig config;
        config.FontData             = font_data.GetData();
        config.FontDataSize         = font_data.GetDataSize();
        config.SizePixels           = g_App->GetDpiRatio() * 18.0f;
        config.FontDataOwnedByAtlas = false;  // the font data is owned by our engin.

        std::u8string name = u8"Hasklig-Regular";
        std::copy_n(name.data(), std::min(name.size(), std::size(config.Name)), config.Name);
        io.Fonts->AddFont(&config);
    }

    unsigned char* pixels = nullptr;
    int            width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    const uint32_t bitcount = 32;
    const size_t   pitch    = width * bitcount / 8;
    m_FontTexture           = std::make_shared<Asset::Image>(width, height, 32, pitch, pitch * height);
    uint8_t* p_texture      = m_FontTexture->GetData();
    std::copy_n(reinterpret_cast<const uint8_t*>(pixels), m_FontTexture->GetDataSize(), p_texture);
}

void GuiManager::MapKey() {
    auto& io = ImGui::GetIO();

    io.KeyMap[ImGuiKey_Tab]         = static_cast<int>(VirtualKeyCode::KEY_TAB);
    io.KeyMap[ImGuiKey_LeftArrow]   = static_cast<int>(VirtualKeyCode::KEY_LEFT);
    io.KeyMap[ImGuiKey_RightArrow]  = static_cast<int>(VirtualKeyCode::KEY_RIGHT);
    io.KeyMap[ImGuiKey_UpArrow]     = static_cast<int>(VirtualKeyCode::KEY_UP);
    io.KeyMap[ImGuiKey_DownArrow]   = static_cast<int>(VirtualKeyCode::KEY_DOWN);
    io.KeyMap[ImGuiKey_PageUp]      = static_cast<int>(VirtualKeyCode::KEY_PGAE_UP);
    io.KeyMap[ImGuiKey_PageDown]    = static_cast<int>(VirtualKeyCode::KEY_PAGE_DOWN);
    io.KeyMap[ImGuiKey_Home]        = static_cast<int>(VirtualKeyCode::KEY_HOME);
    io.KeyMap[ImGuiKey_End]         = static_cast<int>(VirtualKeyCode::KEY_END);
    io.KeyMap[ImGuiKey_Insert]      = static_cast<int>(VirtualKeyCode::KEY_INS);
    io.KeyMap[ImGuiKey_Delete]      = static_cast<int>(VirtualKeyCode::KEY_DEL);
    io.KeyMap[ImGuiKey_Backspace]   = static_cast<int>(VirtualKeyCode::KEY_BACK);
    io.KeyMap[ImGuiKey_Space]       = static_cast<int>(VirtualKeyCode::KEY_SPACE);
    io.KeyMap[ImGuiKey_Enter]       = static_cast<int>(VirtualKeyCode::KEY_ENTER);
    io.KeyMap[ImGuiKey_Escape]      = static_cast<int>(VirtualKeyCode::KEY_ESC);
    io.KeyMap[ImGuiKey_KeyPadEnter] = static_cast<int>(VirtualKeyCode::KEY_ENTER);
    io.KeyMap[ImGuiKey_A]           = static_cast<int>(VirtualKeyCode::KEY_A);
    io.KeyMap[ImGuiKey_C]           = static_cast<int>(VirtualKeyCode::KEY_C);
    io.KeyMap[ImGuiKey_V]           = static_cast<int>(VirtualKeyCode::KEY_V);
    io.KeyMap[ImGuiKey_X]           = static_cast<int>(VirtualKeyCode::KEY_X);
    io.KeyMap[ImGuiKey_Y]           = static_cast<int>(VirtualKeyCode::KEY_Y);
    io.KeyMap[ImGuiKey_Z]           = static_cast<int>(VirtualKeyCode::KEY_Z);
}

}  // namespace Hitagi::Gui
