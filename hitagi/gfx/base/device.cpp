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

class Device : public core::RuntimeModule {
public:
    enum struct Type : std::uint8_t {
        DX12,
        Vulkan,
        Mock
    } const device_type;

    ~Device() override;

    void Tick() override;

    inline void SetProfile(bool enable) noexcept { m_EnableProfile = enable; }
    inline auto GetLogger() const noexcept -> std::shared_ptr<spdlog::logger> { return m_Logger; }

protected:
    Device(Type type, std::string_view name);

    std::function<void()> report_debug_error_after_destroy_fn;

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
    : core::RuntimeModule(std::format("{}{}", magic_enum::enum_name(type), utils::add_parentheses(name))),
      device_type(type) {}

Device::~Device() {
    if (report_debug_error_after_destroy_fn) {
        report_debug_error_after_destroy_fn();
    }
    m_Logger->trace("graphics device removed successfully!");
}

void Device::Tick() {
    core::RuntimeModule::Tick();
    m_FrameIndex++;
}

}  // namespace hitagi::gfx
