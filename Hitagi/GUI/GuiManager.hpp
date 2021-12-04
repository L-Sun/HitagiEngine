#pragma once
#include "IRuntimeModule.hpp"
#include "Timer.hpp"
#include "Image.hpp"

#include <imgui.h>

#include <list>

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
    void LoadFontTexture();
    void MapKey();

    Core::Clock                   m_Clock;
    std::shared_ptr<Asset::Image> m_FontTexture;
    std::list<Core::Buffer>       m_FontsData;
};

}  // namespace Hitagi::Gui

namespace Hitagi {
extern std::unique_ptr<Gui::GuiManager> g_GuiManager;
}
