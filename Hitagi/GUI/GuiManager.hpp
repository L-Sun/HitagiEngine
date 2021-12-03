#pragma once
#include "IRuntimeModule.hpp"
#include "Timer.hpp"
#include "Image.hpp"

#include <imgui.h>

namespace Hitagi::Gui {
class GuiManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    inline ImDrawData* GetGuiDrawData() {
        return ImGui::GetDrawData();
    }

    inline auto GetGuiFontTexture() const noexcept { return m_FontTexture; }

private:
    std::shared_ptr<Asset::Image> LoadFontTexture();
    void                          MapKey();

    Core::Clock                   m_Clock;
    std::shared_ptr<Asset::Image> m_FontTexture;
};

}  // namespace Hitagi::Gui

namespace Hitagi {
extern std::unique_ptr<Gui::GuiManager> g_GuiManager;
}
