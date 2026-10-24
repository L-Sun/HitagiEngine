module;
#include "interop/tracy_macros.hpp"

export module gfx.metal:sync;
import interop.metal;
import interop.spdlog;
import interop.tracy;
import std;
import gfx.base;
import :types;
import :utils;

export namespace hitagi::gfx {

class MetalFence final : public Fence {
public:
    MetalFence(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger,
               std::uint64_t initial_value, std::string_view name);
    void         Signal(std::uint64_t value) final;
    bool         Wait(std::uint64_t value, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) final;
    auto         GetCurrentValue() -> std::uint64_t final;
    inline auto& GetEvent() const noexcept { return m_Event; }
    inline auto  GetNativeDevice() const noexcept { return &m_Device; }

private:
    const ns::SharedPtr<mtl::Device>& m_Device;
    ns::SharedPtr<mtl::SharedEvent>   m_Event;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

MetalFence::MetalFence(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger,
                       std::uint64_t initial_value, std::string_view name)
    : Fence(name), m_Device(device), m_Event(ns::TransferPtr(device->newSharedEvent())) {
    check_metal(logger, m_Event.get() != nullptr, "Create Metal timeline event failed");
    m_Event->setSignaledValue(initial_value);
}

void MetalFence::Signal(std::uint64_t value) {
    m_Event->setSignaledValue(value);
}

bool MetalFence::Wait(std::uint64_t value, std::chrono::milliseconds timeout) {
    ZoneScoped;
    if (GetCurrentValue() >= value) return true;
    if (timeout <= std::chrono::milliseconds::zero()) return false;
    // Use bounded native waits for an unbounded host wait, avoiding timeout overflow.
    if (timeout == std::chrono::milliseconds::max()) {
        while (!m_Event->waitUntilSignaledValue(value, 1000)) {
        }
        return true;
    }
    return m_Event->waitUntilSignaledValue(value, static_cast<std::uint64_t>(timeout.count()));
}

auto MetalFence::GetCurrentValue() -> std::uint64_t { return m_Event->signaledValue(); }

}  // namespace hitagi::gfx
