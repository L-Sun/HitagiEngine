#pragma once
#include <hitagi/gfx/gpu_resource.hpp>

namespace hitagi::gfx {
class Device;

struct BindlessHandle {
    std::uint32_t index;
    std::uint16_t version;
    std::uint8_t  type;
    std::uint8_t  tag;
};

// This struct is used of push constant, which will refer to a GPUBuffer containing bindless handles
struct BindlessInfoOffset {
    BindlessHandle bindless_info_handle;
    std::uint32_t  user_data_0;
    std::uint32_t  user_data_1;
};

struct GPUBufferView {
    GPUBuffer&    buffer;
    std::uint64_t offset;
    std::uint64_t size;
};

class BindlessUtils {
public:
    BindlessUtils(Device& device, std::string_view name) : m_Device(device), m_Name(name) {}
    virtual ~BindlessUtils() = default;

    [[nodiscard]] virtual auto CreateBindlessHandle(GPUBuffer& buffer, std::size_t index = 0, bool writable = false) -> BindlessHandle = 0;
    [[nodiscard]] virtual auto CreateBindlessHandle(Texture& texture, bool writeable = false) -> BindlessHandle                        = 0;
    virtual void               DiscardBindlessHandle(BindlessHandle handle)                                                            = 0;

    inline auto& GetDevice() const noexcept { return m_Device; }
    inline auto  GetName() const noexcept { return std::string_view(m_Name); }

protected:
    Device&          m_Device;
    std::pmr::string m_Name;
};

}  // namespace hitagi::gfx