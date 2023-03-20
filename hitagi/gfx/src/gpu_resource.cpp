#include <hitagi/gfx/gpu_resource.hpp>
#include <hitagi/gfx/device.hpp>
#include <hitagi/utils/flags.hpp>

#include <spdlog/logger.h>
#include <fmt/color.h>

namespace hitagi::gfx {

void GPUBuffer::UpdateRaw(std::size_t index, std::span<const std::byte> data) {
    auto mapped_ptr = GetMappedPtr();
    if (mapped_ptr == nullptr) {
        m_Device.GetLogger()->error(
            "Can not update buffer({}) because it is not mapped!",
            fmt::styled(m_Name.c_str(), fmt::fg(fmt::color::red)));
        return;
    }
    if (data.size_bytes() > m_Desc.element_size) {
        m_Device.GetLogger()->error(
            "Can not update buffer({}) at index({}) because the data size({}) large than element size({})!",
            fmt::styled(m_Name.c_str(), fmt::fg(fmt::color::red)),
            fmt::styled(index, fmt::fg(fmt::color::red)),
            fmt::styled(data.size_bytes(), fmt::fg(fmt::color::red)),
            fmt::styled(m_Desc.element_size, fmt::fg(fmt::color::green)));
        return;
    }
    std::memcpy(mapped_ptr + index * m_Desc.element_size, data.data(), data.size_bytes());
}

auto Shader::GetDXILData() const noexcept -> std::span<const std::byte> {
    return {};
}

auto Shader::GetSPIRVData() const noexcept -> std::span<const std::byte> {
    return {};
}

}  // namespace hitagi::gfx