#pragma once
#include <hitagi/resource/texture.hpp>
#include <hitagi/resource/scene.hpp>
#include <hitagi/graphics/graphics_manager.hpp>

namespace hitagi {
class SceneViewPort : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;
    void Finalize() final;

    inline std::string_view GetName() const noexcept final { return "SceneViewPort"; }

    void SetScene(std::shared_ptr<resource::Scene> scene);

private:
    bool m_Open = true;

    std::shared_ptr<resource::Scene>                                                   m_CurrentScene = nullptr;
    std::array<std::shared_ptr<resource::Texture>, graphics_manager->sm_SwapChianSize> m_RenderTextures;
};
}  // namespace hitagi