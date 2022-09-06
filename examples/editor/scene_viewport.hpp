#pragma once
#include <hitagi/resource/texture.hpp>
#include <hitagi/resource/scene.hpp>

namespace hitagi {
class SceneViewPort : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;
    void Finalize() final;

    inline std::string_view GetName() const noexcept final { return "SceneViewPort"; }

    void SetScene(std::shared_ptr<resource::Scene> scene);

private:
    bool        m_Open     = true;
    math::vec2u m_PrevSize = {0, 0};

    std::shared_ptr<resource::Scene>   m_CurrentScene = nullptr;
    std::shared_ptr<resource::Texture> m_RenderTexture;
};
}  // namespace hitagi