module;

#include <imgui.h>
#include <spdlog/spdlog.h>

module editor;

namespace hitagi {

ImageViewer::ImageViewer(const Engine& engine)
    : core::RuntimeModule("ImageViewer"),
      m_RenderGraph(engine.Renderer().GetRenderGraph()),
      m_GuiManager(engine.GuiManager()) {}

void ImageViewer::SetTexture(const std::shared_ptr<asset::Texture>& texture) {
    m_CurrentTexture = texture;
    m_IsOpen         = true;
}

void ImageViewer::Tick() {
    if (m_IsOpen) {
        m_GuiManager.DrawGui([&]() {
            if (ImGui::Begin("Image Viewer", &m_IsOpen)) {
                if (const auto texture = m_CurrentTexture.lock()) {
                    ImGui::Image(m_GuiManager.ReadTexture(m_RenderGraph.Import(texture->GetGPUData())), ImVec2(texture->Width(), texture->Height()));
                }

                ImGui::End();
            }
        });
    }
    core::RuntimeModule::Tick();
}

}  // namespace hitagi
