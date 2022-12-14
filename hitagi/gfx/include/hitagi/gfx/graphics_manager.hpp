#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/gfx/device.hpp>
#include <hitagi/gfx/render_graph.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

namespace hitagi::gfx {
class GraphicsManager final : public RuntimeModule {
public:
    GraphicsManager();
    ~GraphicsManager() final;
    void Tick() final;

    inline auto& GetDevice() const noexcept { return *m_Device; }
    inline auto& GetRenderGraph() const noexcept { return *m_RenderGraph; };
    inline auto& GetSwapChain() const noexcept { return *m_SwapChain; }

    inline auto GetFrameTime() const noexcept { return m_Clock.DeltaTime(); }

private:
    core::Clock   m_Clock;
    std::uint64_t m_FrameIndex = 0;

    std::unique_ptr<Device>      m_Device;
    std::shared_ptr<SwapChain>   m_SwapChain;
    std::unique_ptr<RenderGraph> m_RenderGraph;

    utils::EnumArray<std::uint64_t, CommandType> m_LastFenceValues = utils::create_enum_array<std::uint64_t, CommandType>(0);
};

}  // namespace hitagi::gfx
namespace hitagi {
extern gfx::GraphicsManager* graphics_manager;
}