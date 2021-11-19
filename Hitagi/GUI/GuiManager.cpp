#include "GuiManager.hpp"
#include "Application.hpp"

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
    auto& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();

    unsigned char* pixels = nullptr;
    int            width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    const uint32_t bitcount = 32;
    const size_t   pitch    = width * bitcount / 8;
    m_FontTexture           = std::make_shared<Asset::Image>(width, height, 32, pitch, pitch * height);
    uint8_t* p_texture      = m_FontTexture->GetData();
    std::copy_n(reinterpret_cast<const uint8_t*>(pixels), m_FontTexture->GetDataSize(), p_texture);
    return 0;
}

void GuiManager::Tick() {
    m_Clock.Tick();
    {
        auto  rect       = g_App->GetWindowsRect();
        auto& io         = ImGui::GetIO();
        io.DisplaySize.x = rect.right - rect.left;
        io.DisplaySize.y = rect.bottom - rect.top;
        io.DeltaTime     = m_Clock.DeltaTime().count();
    }
    ImGui::NewFrame();
    bool show_demo_window = true;
    ImGui::ShowDemoWindow(&show_demo_window);
    ImGui::Render();
}

void GuiManager::Finalize() {
    ImGui::DestroyContext();
}
}  // namespace Hitagi::Gui
