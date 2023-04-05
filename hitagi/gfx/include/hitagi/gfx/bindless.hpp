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
};

struct BindlessMetaInfo {
    BindlessHandle handle = {};
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
    [[nodiscard]] virtual auto CreateBindlessHandle(Sampler& sampler) -> BindlessHandle                                                = 0;
    virtual void               DiscardBindlessHandle(BindlessHandle handle)                                                            = 0;

    inline auto& GetDevice() const noexcept { return m_Device; }
    inline auto  GetName() const noexcept { return std::string_view(m_Name); }

protected:
    Device&          m_Device;
    std::pmr::string m_Name;
};

}  // namespace hitagi::gfx