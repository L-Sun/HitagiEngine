#pragma once
#include "resource_manager.hpp"

#include <hitagi/graphics/renderable.hpp>
#include <hitagi/resource/camera.hpp>
#include <hitagi/resource/light.hpp>

namespace hitagi::graphics {
class DriverAPI;
class IGraphicsCommandContext;

class Frame {
    struct FrameConstant {
        math::mat4f proj_view;
        math::mat4f view;
        math::mat4f projection;
        math::mat4f inv_projection;
        math::mat4f inv_view;
        math::mat4f inv_proj_view;
        math::vec4f camera_pos;
        math::vec4f light_position;
        math::vec4f light_pos_in_view;
        math::vec4f light_intensity;
    };

    struct ObjectConstant {
        math::mat4f word_transform;
        math::mat4f object_transform;
        math::mat4f parent_transform;
        math::vec4f translation;
        math::quatf rotation;
        math::vec4f scaling;
    };

public:
    Frame(DriverAPI& driver, ResourceManager& resource_manager, std::size_t frame_index);

    void SetFenceValue(std::uint64_t fence_value);
    // TODO generate pipeline state object from scene node infomation
    void AddRenderables(std::pmr::vector<Renderable> renderables);

    void SetCamera(std::shared_ptr<resource::Camera> camera);
    void SetLight(std::shared_ptr<resource::Light> light);

    std::shared_ptr<resource::Camera> GetCamera() const;

    void Draw(IGraphicsCommandContext* context);

    bool IsRenderingFinished() const;
    bool Locked() const { return m_Lock; }
    void Reset();

    void        SetRenderTarget(std::shared_ptr<RenderTarget> rt);
    inline auto GetRenderTarget() noexcept { return m_Output; }

private:
    void PrepareData();

    DriverAPI&        m_Driver;
    ResourceManager&  m_ResMgr;
    const std::size_t m_FrameIndex;
    std::uint64_t     m_FenceValue = 0;
    bool              m_Lock       = false;

    FrameConstant                     m_FrameConstant{};
    std::shared_ptr<resource::Camera> m_Camera;
    std::pmr::vector<Renderable>      m_Renderables;
    std::shared_ptr<RenderTarget>     m_Output;

    // the constant data used among the frame, including camera, light, etc.
    // TODO object constant buffer layout depending on different object
    std::shared_ptr<ConstantBuffer> m_ConstantBuffer;
    // the first element in constant buffer is frame constant, including camera light
    std::size_t m_ConstantCount = 1;
};

}  // namespace hitagi::graphics
