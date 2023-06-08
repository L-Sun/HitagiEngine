#include "dx12_sync.hpp"
#include "dx12_device.hpp"

#include <spdlog/logger.h>
#include <fmt/color.h>

namespace hitagi::gfx {
DX12Fence::DX12Fence(DX12Device& device, std::uint64_t initial_value, std::string_view name) : Fence(device, name) {
    const auto logger = device.GetLogger();

    logger->trace("Creating Fence: {}", fmt::styled(name, fmt::fg(fmt::color::green)));
    if (FAILED(device.GetDevice()->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)))) {
        const auto error_message = fmt::format(
            "Failed to create Fence({})",
            fmt::styled(name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    if (FAILED(m_Fence->SetName(std::wstring(name.begin(), name.end()).c_str()))) {
        logger->warn(
            "Failed to set name to Fence({})",
            fmt::styled(name, fmt::fg(fmt::color::red)));
    }

    if (m_EventHandle = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
        m_EventHandle == nullptr) {
        const auto error_message = fmt::format(
            "Failed to create Fence({}) event handle",
            fmt::styled(name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
}

DX12Fence::~DX12Fence() {
    CloseHandle(m_EventHandle);
}

void DX12Fence::Signal(std::uint64_t value) {
    m_Fence->Signal(value);
}

bool DX12Fence::Wait(std::uint64_t value, std::chrono::milliseconds timeout) {
    auto dx12_device = static_cast<DX12Device&>(m_Device).GetDevice();
    if (m_Fence->GetCompletedValue() < value) {
        m_Fence->SetEventOnCompletion(value, m_EventHandle);
        return WAIT_TIMEOUT != WaitForSingleObject(m_EventHandle, timeout.count());
    }
    return true;
}

auto DX12Fence::GetCurrentValue() -> std::uint64_t {
    auto output = m_Fence->GetCompletedValue();
    return output;
}

}  // namespace hitagi::gfx