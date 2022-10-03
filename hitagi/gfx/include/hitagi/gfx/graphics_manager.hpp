#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/gfx/device.hpp>

namespace hitagi::gfx {
class GraphicsManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Finalize() final;
    void Tick() final;

    inline std::string_view GetName() const noexcept final { return "GraphicsManager"; }

    inline Device* GetDevice() const noexcept { return m_Device.get(); }

private:
    std::unique_ptr<Device> m_Device;
};

}  // namespace hitagi::gfx
namespace hitagi {
extern gfx::GraphicsManager* graphics_manager;
}