#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/asset/scene.hpp>
#include <hitagi/gfx/render_graph.hpp>
#include <hitagi/application.hpp>
#include <hitagi/gui/gui_manager.hpp>

namespace hitagi::render {
class IRenderer : public RuntimeModule {
public:
    using RuntimeModule::RuntimeModule;

    virtual auto RenderScene(const asset::Scene& scene, const gfx::ViewPort& viewport, std::shared_ptr<asset::CameraNode> camera = nullptr) -> gfx::ResourceHandle = 0;
    virtual auto GetFrameTime() const noexcept -> std::chrono::duration<double>                                                                                    = 0;
};

}  // namespace hitagi::render