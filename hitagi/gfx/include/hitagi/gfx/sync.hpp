#pragma once
#include <chrono>
#include <string>
#include <string_view>
#include <memory_resource>
#include <memory>

namespace hitagi::gfx {
class Device;

class Fence {
public:
    Fence(Device& device, std::string_view name = "") : m_Device(device), m_Name(name) {}
    Fence(const Fence&)            = delete;
    Fence(Fence&&)                 = default;
    Fence& operator=(const Fence&) = delete;
    Fence& operator=(Fence&&)      = delete;
    virtual ~Fence()               = default;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }

    virtual void Wait(std::chrono::duration<double> timeout = std::chrono::duration<double>::max()) = 0;

protected:
    Device&          m_Device;
    std::pmr::string m_Name;
};

class Semaphore {
public:
    Semaphore(Device& device, std::string_view name = "") : m_Device(device), m_Name(name) {}
    Semaphore(const Semaphore&)            = delete;
    Semaphore(Semaphore&&)                 = default;
    Semaphore& operator=(const Semaphore&) = delete;
    Semaphore& operator=(Semaphore&&)      = delete;
    virtual ~Semaphore()                   = default;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }

protected:
    Device&          m_Device;
    std::pmr::string m_Name;
};

}  // namespace hitagi::gfx