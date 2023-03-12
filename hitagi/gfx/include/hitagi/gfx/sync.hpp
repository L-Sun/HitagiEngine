#pragma once
#include <chrono>
#include <string>
#include <string_view>
#include <memory_resource>
#include <memory>

namespace hitagi::gfx {
class Device;

class Semaphore {
public:
    Semaphore(Device& device, std::string_view name = "") : m_Device(device), m_Name(name) {}
    Semaphore(const Semaphore&)            = delete;
    Semaphore(Semaphore&&)                 = default;
    Semaphore& operator=(const Semaphore&) = delete;
    Semaphore& operator=(Semaphore&&)      = delete;
    virtual ~Semaphore()                   = default;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }

    virtual void Signal(std::uint64_t value) = 0;

    virtual void Wait(std::uint64_t value) = 0;

    // return true if the fence is signaled within the timeout
    virtual bool WaitFor(std::uint64_t value, std::chrono::duration<double> timeout) = 0;

protected:
    Device&          m_Device;
    std::pmr::string m_Name;
};

using SemaphoreWaitPair = std::pair<std::shared_ptr<Semaphore>, std::uint64_t>;

}  // namespace hitagi::gfx