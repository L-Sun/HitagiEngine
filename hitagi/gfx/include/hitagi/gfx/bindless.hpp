#pragma once
#include <hitagi/gfx/gpu_resource.hpp>

namespace hitagi::gfx {
class Device;

enum struct BindlessHandleType : std::uint32_t {
    Buffer,
    Texture,
    Sampler,
    Invalid,
};

// TODO use byte array buffer to store bindless handle with bit field
struct BindlessHandle {
    std::uint32_t      index;
    BindlessHandleType type     = BindlessHandleType::Invalid;
    std::uint32_t      writable = 0;
    std::uint32_t      version  = 0;

    inline operator bool() const noexcept {
        return type != BindlessHandleType::Invalid;
    }
};

struct BindlessMetaInfo {
    BindlessHandle handle = {};
};

class BindlessUtils {
public:
    virtual ~BindlessUtils() = default;

    [[nodiscard]] virtual auto CreateBindlessHandle(GPUBuffer& buffer, std::uint64_t index, bool writable = false) -> BindlessHandle = 0;
    [[nodiscard]] virtual auto CreateBindlessHandle(Texture& texture, bool writeable = false) -> BindlessHandle                      = 0;
    [[nodiscard]] virtual auto CreateBindlessHandle(Sampler& sampler) -> BindlessHandle                                              = 0;
    virtual void               DiscardBindlessHandle(BindlessHandle handle)                                                          = 0;

    inline auto& GetDevice() const noexcept { return m_Device; }
    inline auto  GetName() const noexcept { return std::string_view(m_Name); }

protected:
    BindlessUtils(Device& device, std::string_view name) : m_Device(device), m_Name(name) {}

    Device&          m_Device;
    std::pmr::string m_Name;
};

}  // namespace hitagi::gfx