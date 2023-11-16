#pragma once
#include <hitagi/engine.hpp>

namespace hitagi {
class ImageViewer : public RuntimeModule {
public:
    ImageViewer(const Engine& engine);
    void SetTexture(const std::shared_ptr<asset::Texture>& texture = nullptr);
    void Tick();

private:
    bool                          m_IsOpen = true;
    rg::RenderGraph&              m_RenderGraph;
    gui::GuiManager&              m_GuiManager;
    std::weak_ptr<asset::Texture> m_CurrentTexture;
};
}  // namespace hitagi