#pragma once
#include <hitagi/math/matrix.hpp>
#include <hitagi/resource/scene.hpp>
#include <hitagi/graphics/gpu_resource.hpp>

namespace hitagi::graphics {
class DeviceAPI;
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
    };

public:
    Frame(DeviceAPI& device, std::size_t frame_index);

    void SetFenceValue(std::uint64_t fence_value);

    void AppendRenderables(std::pmr::vector<resource::Renderable> renderables);

    void SetCamera(resource::Camera camera);

    void PrepareData();
    void Render(IGraphicsCommandContext* context, resource::Renderable::Type type);

    bool        IsRenderingFinished() const;
    inline bool Locked() const { return m_Lock; }
    void        Reset();

    inline auto& GetRenderTarget() noexcept { return m_Output; }

private:
    ConstantBuffer& GetMaterialBuffer(const std::shared_ptr<resource::Material>& material);

    DeviceAPI&        m_Device;
    const std::size_t m_FrameIndex;
    std::uint64_t     m_FenceValue = 0;
    bool              m_Lock       = false;

    FrameConstant                          m_FrameConstant{};
    std::pmr::vector<resource::Renderable> m_RenderItems;
    RenderTarget                           m_Output;

    // the constant data used among the frame, including camera, light, etc.
    // TODO object constant buffer layout depending on different object
    ConstantBuffer m_ConstantBuffer;
    // the first element in constant buffer is frame constant, including camera light
    std::size_t m_ConstantCount = 1;

    std::pmr::unordered_map<std::shared_ptr<resource::Material>, ConstantBuffer> m_MaterialBuffers;
};

}  // namespace hitagi::graphics
