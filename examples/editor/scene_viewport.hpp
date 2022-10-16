#pragma once
#include <hitagi/asset/texture.hpp>
#include <hitagi/asset/scene.hpp>
#include <hitagi/gfx/render_graph.hpp>

namespace hitagi {
class SceneViewPort : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;

    inline std::string_view GetName() const noexcept final { return "SceneViewPort"; }

    void SetScene(std::shared_ptr<asset::Scene> scene);

private:
    bool                          m_Open         = true;
    std::shared_ptr<asset::Scene> m_CurrentScene = nullptr;
};
}  // namespace hitagi