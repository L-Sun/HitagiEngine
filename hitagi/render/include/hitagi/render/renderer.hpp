#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/asset/scene.hpp>
#include <hitagi/gfx/device.hpp>

namespace hitagi::render {
class IRenderer : public RuntimeModule {
public:
    virtual void RenderScene(const asset::Scene& scene);
};

class Renderer : public IRenderer {
public:
    Renderer(gfx::Device::Type gfx_device_type);

private:
    std::unique_ptr<gfx::Device> m_GfxDevice;
};

}  // namespace hitagi::render