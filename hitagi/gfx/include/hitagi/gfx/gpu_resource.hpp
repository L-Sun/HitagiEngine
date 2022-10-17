#pragma once
#include <hitagi/gfx/common_types.hpp>
#include <hitagi/utils/utils.hpp>
#include <hitagi/core/buffer.hpp>

#include <functional>

namespace hitagi::gfx {
class CommandQueue;
class Device;

constexpr auto UNKOWN_NAME = "Unkown";
struct Resource {
    virtual ~Resource() = default;
    Resource(Device& device) : device(device) {}
    Device& device;
};

struct GpuBuffer : public Resource {
    enum struct UsageFlags : std::uint32_t {
        Unkown = 0,
        // CPU can read data from mapped pointer
        MapRead = 0x1,
        // CPU can write data to mapped pointer
        MapWrite = (MapRead << 1),
        CopySrc  = (MapWrite << 1),
        CopyDst  = (CopySrc << 1),
        Constant = (CopyDst << 1),
    };

    struct Desc {
        std::string_view name = UNKOWN_NAME;
        std::uint64_t    size;
        UsageFlags       usages = UsageFlags::Unkown;
    };

    GpuBuffer(Device& device, Desc desc, std::byte* mapped_ptr) : Resource(device), desc(desc), mapped_ptr(mapped_ptr) {}

    const Desc       desc;
    std::byte* const mapped_ptr;
};

struct GpuBufferView : public Resource {
    enum struct UsageFlags : std::uint32_t {
        Vertex   = 0x1,
        Index    = (Vertex << 1),
        Constant = (Index << 1),
        Indirect = (Constant << 1),
    };

    struct Desc {
        GpuBuffer&  buffer;
        std::size_t offset = 0;
        std::size_t size   = 0;  // When `size == 0`, it will be re-calculated with `buffer.size - offset`
        std::size_t stride;
        UsageFlags  usages;
    };

    GpuBufferView(Device& device, Desc desc) : Resource(device), desc(desc) {}

    const Desc desc;
};

struct Texture : public Resource {
    enum struct UsageFlags : std::uint8_t {
        CopySrc = 0x1,
        CopyDst = (CopySrc << 1),
        SRV     = (CopyDst << 1),
        UAV     = (SRV << 1),
        RTV     = (UAV << 1),
        DSV     = (RTV << 1),
    };

    struct Desc {
        std::string_view name         = UNKOWN_NAME;
        std::uint32_t    width        = 1;
        std::uint32_t    height       = 1;
        std::uint16_t    depth        = 1;
        std::uint16_t    array_size   = 1;
        Format           format       = Format::UNKNOWN;
        std::uint16_t    mip_levels   = 1;
        std::uint32_t    sample_count = 1;
        ClearValue       clear_value;
        UsageFlags       usages = UsageFlags::SRV;
    };

    Texture(Device& device, Desc desc) : Resource(device), desc(desc) {}

    const Desc desc;
};

struct TextureView : public Resource {
    struct Desc {
        Texture&      textuer;
        Format        format            = Format::UNKNOWN;
        bool          is_cube           = false;
        std::uint16_t base_mip_level    = 0;
        std::uint16_t mip_level_count   = 1;
        std::uint32_t base_array_layer  = 0;
        std::uint32_t array_layer_count = 1;
    };

    TextureView(Device& device, Desc desc) : Resource(device), desc(desc) {}

    const Desc desc;
};

struct Sampler : public Resource {
    enum struct AddressMode : std::uint8_t {
        Clamp,
        Repeat,
        MirrorRepeat
    };
    enum struct FilterMode {
        Point,
        Linear
    };

    struct Desc {
        std::string_view name           = UNKOWN_NAME;
        AddressMode      address_u      = AddressMode::Clamp;
        AddressMode      address_v      = AddressMode::Clamp;
        AddressMode      address_w      = AddressMode::Clamp;
        FilterMode       mag_filter     = FilterMode::Point;
        FilterMode       min_filter     = FilterMode::Point;
        FilterMode       mipmap_filter  = FilterMode::Point;
        float            min_lod        = 0;
        float            max_load       = 32;
        std::uint32_t    max_anisotropy = 1;
        CompareFunction  compare;
    };

    Sampler(Device& device, Desc desc) : Resource(device), desc(desc) {}

    const Desc desc;
};

struct SwapChain : public Resource {
    struct Desc {
        std::string_view name = UNKOWN_NAME;
        void*            window_ptr;
        std::uint8_t     frame_count  = 2;
        Format           format       = Format::R8G8B8A8_UNORM;
        std::uint32_t    sample_count = 1;
        bool             vsync        = false;
    };

    SwapChain(Device& device, Desc desc) : Resource(device), desc(desc) {}

    const Desc desc;

    virtual auto GetCurrentBackIndex() -> std::uint8_t     = 0;
    virtual auto GetCurrentBackBuffer() -> Texture&        = 0;
    virtual auto GetBuffer(std::uint8_t index) -> Texture& = 0;
    virtual void Present()                                 = 0;
    virtual void Resize()                                  = 0;
};

struct Shader : public Resource {
    enum struct Type : std::uint8_t {
        Vertex,
        Pixel,
        Geometry,
        Compute,
    };

    struct Desc {
        std::string_view name = UNKOWN_NAME;
        Type             type;
        std::string_view entry;
        std::string_view source_code;
    };

    Shader(Device& device, Desc desc, core::Buffer binary_data) : Resource(device), desc(desc), binary_data(std::move(binary_data)) {}

    const Desc desc;
    // Complied result
    const core::Buffer binary_data;
};

struct RenderPipeline : public Resource {
    struct Desc {
        std::string_view name = UNKOWN_NAME;
        // Shader config
        std::shared_ptr<Shader> vs;  // vertex shader
        std::shared_ptr<Shader> ps;  // pixel shader
        std::shared_ptr<Shader> gs;  // geometry shader
        std::shared_ptr<Shader> ds;
        std::shared_ptr<Shader> hs;

        PrimitiveTopology     topology = PrimitiveTopology::TriangleList;
        InputLayout           input_layout;
        RasterizerDescription rasterizer_config;
        BlendDescription      blend_config;
        Format                render_format        = Format::R8G8B8A8_UNORM;
        Format                depth_sentcil_format = Format::UNKNOWN;
    };

    RenderPipeline(Device& device, Desc desc) : Resource(device), desc(std::move(desc)) {}

    const Desc desc;
};

struct ComputePipeline : public Resource {
    struct Desc {
        std::shared_ptr<Shader> cs;  // computer shader
    };

    ComputePipeline(Device& device, Desc desc) : Resource(device), desc(std::move(desc)) {}

    const Desc desc;
};

}  // namespace hitagi::gfx

template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::GpuBuffer::UsageFlags> {
    static constexpr bool enable = true;
};
template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::GpuBufferView::UsageFlags> {
    static constexpr bool enable = true;
};
template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::Texture::UsageFlags> {
    static constexpr bool enable = true;
};
