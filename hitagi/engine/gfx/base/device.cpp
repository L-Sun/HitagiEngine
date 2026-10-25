export module gfx.base:device;
import interop.spdlog;
import interop.magic_enum;

import std;
import core;
import utils;
import :types;
import :utils;

export namespace hitagi::gfx {

class GPUBuffer;

class Device {
public:
    enum struct Type : std::uint8_t {
        DX12,
        Metal,
        Vulkan,
        Mock,
    } const device_type;

    virtual ~Device();

    virtual void BeginFrame();
    auto GetName() const noexcept -> std::string_view { return m_Name; }

    inline void SetProfile(bool enable) noexcept { m_EnableProfile = enable; }
    inline auto GetLogger() const noexcept -> std::shared_ptr<spdlog::logger> { return m_Logger; }

protected:
    Device(Type type, std::string_view name);

    std::function<void()> report_debug_error_after_destroy_fn;

    std::pmr::string m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;
    std::size_t m_FrameIndex    = 0;
    bool        m_EnableProfile = false;

private:
    friend GPUBuffer;
    // Backend dispatch for GPUBuffer's static query; not a public Device capability API.
    virtual auto GetStorageBufferViewRequirements() const noexcept -> StorageViewRequirements {
        return {.offset_alignment = 4, .size_alignment = 4};
    }
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

Device::Device(Type type, std::string_view name)
    : device_type(type),
      m_Name(std::format("{}{}", magic_enum::enum_name(type), utils::add_parentheses(name))),
      m_Logger(utils::try_create_logger(m_Name)) {}

Device::~Device() {
    if (report_debug_error_after_destroy_fn) {
        report_debug_error_after_destroy_fn();
    }
    m_Logger->trace("graphics device removed successfully!");
}

void Device::BeginFrame() {
    m_FrameIndex++;
}

}  // namespace hitagi::gfx
