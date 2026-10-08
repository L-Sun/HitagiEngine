module;
#include <d3d12.h>
#include <wrl.h>
#include <spdlog/logger.h>
#include <fmt/color.h>
#include <d3dx12/d3dx12.h>
#include <tracy/Tracy.hpp>

export module gfx.dx12:sync;
import std;
import core;
import utils;
import math;
import gfx.base;
import magic_enum;

using namespace Microsoft::WRL;

export namespace hitagi::gfx {

class DX12Fence final : public Fence {
public:
    DX12Fence(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, std::uint64_t initial_value, std::string_view name);
    ~DX12Fence() final;

    void Signal(std::uint64_t value) final;
    bool Wait(std::uint64_t value, std::chrono::milliseconds timeout = (std::chrono::milliseconds::max)()) final;
    auto GetCurrentValue() -> std::uint64_t final;

    inline auto GetFence() const noexcept { return m_Fence; }

private:
    ComPtr<ID3D12Fence> m_Fence;
    void*               m_EventHandle = nullptr;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

DX12Fence::DX12Fence(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, std::uint64_t initial_value, std::string_view name) : Fence(name) {
    logger->trace("Creating Fence: {}", fmt::styled(name, fmt::fg(fmt::color::green)));
    if (FAILED(device.CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)))) {
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
    ZoneScopedNS("DX12Fence::Wait", 8);
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
