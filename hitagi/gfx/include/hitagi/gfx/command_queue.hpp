#pragma once
#include <hitagi/gfx/command_context.hpp>
#include <hitagi/gfx/sync.hpp>

#include <cstdint>
#include <vector>

namespace hitagi::gfx {
class Device;

class CommandQueue {
public:
    CommandQueue(Device& device, CommandType type, std::string_view name) : m_Device(device), m_Type(type), m_Name(name) {}
    virtual ~CommandQueue() = default;

    inline auto& GetDevice() const noexcept { return m_Device; }
    inline auto  GetType() const noexcept { return m_Type; }
    inline auto& GetName() const noexcept { return m_Name; }

    virtual void Submit(
        std::span<const std::reference_wrapper<const CommandContext>> contexts,
        std::span<const FenceWaitInfo>                                wait_fences   = {},
        std::span<const FenceSignalInfo>                              signal_fences = {}) = 0;

    virtual void WaitIdle() = 0;

protected:
    Device&                m_Device;
    const CommandType      m_Type;
    const std::pmr::string m_Name;
};
}  // namespace hitagi::gfx