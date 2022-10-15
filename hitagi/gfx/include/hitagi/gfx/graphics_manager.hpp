#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/gfx/device.hpp>
#include <hitagi/gfx/render_graph.hpp>

namespace hitagi::gfx {
class GraphicsManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Finalize() final;
    void Tick() final;

    inline std::string_view GetName() const noexcept final { return "GraphicsManager"; }

    inline auto& GetDevice() const noexcept { return *m_Device; }
    inline auto& GetRenderGraph() const noexcept { return *m_RenderGraph; };

private:
    std::unique_ptr<Device>      m_Device;
    std::unique_ptr<RenderGraph> m_RenderGraph;

    utils::EnumArray<std::uint64_t, CommandType> m_LastFenceValues = utils::create_enum_array<std::uint64_t, CommandType>(0);
};

}  // namespace hitagi::gfx
namespace hitagi {
extern gfx::GraphicsManager* graphics_manager;
}