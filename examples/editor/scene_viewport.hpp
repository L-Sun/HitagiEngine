#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/asset/texture.hpp>
#include <hitagi/asset/scene.hpp>
#include <hitagi/gfx/render_graph.hpp>

namespace hitagi {
class SceneViewPort : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;

    inline std::string_view GetName() const noexcept final { return "SceneViewPort"; }

    inline void SetScene(std::shared_ptr<asset::Scene> scene) noexcept { m_CurrentScene = std::move(scene); };

    inline auto GetScene() const noexcept { return m_CurrentScene; };

private:
    bool                          m_Open         = true;
    std::shared_ptr<asset::Scene> m_CurrentScene = nullptr;
};
}  // namespace hitagi