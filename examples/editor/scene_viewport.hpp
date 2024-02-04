#pragma once
#include <hitagi/engine.hpp>

namespace hitagi {
class SceneViewPort : public RuntimeModule {
public:
    SceneViewPort(const Engine& engine)
        : RuntimeModule("SceneViewPort"), m_Render(engine.Renderer()), m_GuiManager(engine.GuiManager()) {}

    void Tick() final;

    void SetScene(std::shared_ptr<asset::Scene> scene) noexcept;

    inline auto GetScene() const noexcept { return m_CurrentScene; };

private:
    render::IRenderer&            m_Render;
    gui::GuiManager&              m_GuiManager;
    bool                          m_Open         = true;
    std::shared_ptr<asset::Scene> m_CurrentScene = nullptr;
    ecs::Entity                   m_Camera;
};
}  // namespace hitagi