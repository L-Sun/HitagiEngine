module editor;
import std;

namespace hitagi {
ImageViewer::ImageViewer(const Engine& engine)
    : core::RuntimeModule("ImageViewer"),
      m_RenderGraph(engine.RenderRuntime().GetRenderGraph()),
      m_GuiManager(engine.GuiManager()) {}

void ImageViewer::SetTexture(const std::shared_ptr<asset::Texture>& texture) {
    m_CurrentTexture = texture;
    m_IsOpen         = texture != nullptr;
}

void ImageViewer::Tick() {
    if (m_IsOpen && m_Rect.z > 0 && m_Rect.w > 0) {
        if (const auto texture = m_CurrentTexture.lock()) {
            const auto scale = std::min(m_Rect.z / texture->Width(), m_Rect.w / texture->Height());
            const auto width = texture->Width() * scale, height = texture->Height() * scale;
            m_GuiManager.DrawTexture(m_RenderGraph.Import(texture->GetGPUData()),
                                     {m_Rect.x + (m_Rect.z - width) * 0.5f, m_Rect.y + (m_Rect.w - height) * 0.5f, width, height});
        }
    }
    core::RuntimeModule::Tick();
}
}  // namespace hitagi
