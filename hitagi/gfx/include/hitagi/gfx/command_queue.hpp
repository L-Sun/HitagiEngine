#pragma once
#include <hitagi/gfx/command_context.hpp>

#include <cstdint>
#include <vector>

namespace hitagi::gfx {
class Device;

class CommandQueue {
public:
    CommandQueue(Device* device, CommandType type, std::string_view name)
        : device(device), type(type), name(name) {}
    virtual ~CommandQueue() = default;

    virtual auto Submit(std::pmr::vector<CommandContext*> context) -> std::uint64_t = 0;
    virtual bool IsFenceComplete(std::uint64_t fence_value) const                   = 0;
    virtual void WaitForFence(std::uint64_t fence_value)                            = 0;
    virtual void WaitForQueue(const CommandQueue& other)                            = 0;
    virtual void WaitIdle()                                                         = 0;

    Device* const          device;
    const CommandType      type;
    const std::pmr::string name;
};
}  // namespace hitagi::gfx