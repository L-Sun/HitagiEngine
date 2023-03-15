#pragma once
#include <hitagi/gfx/common_types.hpp>
#include <hitagi/utils/flags.hpp>
#include <hitagi/core/buffer.hpp>
#include <hitagi/utils/types.hpp>
#include <hitagi/utils/hash.hpp>

#include <functional>

namespace hitagi::gfx {
class CommandQueue;
class Device;

constexpr auto UNKOWN_NAME = "Unkown";
class Resource {
public:
    Resource(const Resource&) = delete;
    virtual ~Resource()       = default;

    inline auto  GetName() const noexcept -> std::string_view { return m_Name; }
    inline auto& GetDevice() const noexcept { return m_Device; }

protected:
    Resource(Device& device, std::string_view name = UNKOWN_NAME) : m_Device(device), m_Name(name) {}

    Device&          m_Device;
    std::pmr::string m_Name;
};

class GpuBuffer : public Resource {
public:
    enum struct UsageFlags : std::uint32_t {
        MapRead  = 0x1,             // CPU can read data from mapped pointer
        MapWrite = (MapRead << 1),  // CPU can write data to mapped pointer
        CopySrc  = (MapWrite << 1),
        CopyDst  = (CopySrc << 1),
        Vertex   = (CopyDst << 1),
        Index    = (Vertex << 1),
        Constant = (Index << 1),
    };

    struct Desc {
        std::string_view name = UNKOWN_NAME;
        std::uint64_t    element_size;
        std::uint64_t    element_count = 1;
        UsageFlags       usages;

        inline constexpr bool operator==(const Desc&) const noexcept;
    };

    template <typename T>
    void Update(std::size_t index, T data) {
        UpdateRaw(index, std::span(reinterpret_cast<const std::byte*>(&data), sizeof(T)));
    }
    virtual auto GetMappedPtr() const noexcept -> std::byte* = 0;

    inline const auto& GetDesc() const noexcept { return m_Desc; }

protected:
    void UpdateRaw(std::size_t index, std::span<const std::byte> data);

    GpuBuffer(Device& device, Desc desc);
    Desc m_Desc;
};

class Texture : public Resource {
public:
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
        bool             is_cube      = false;
        ClearValue       clear_value;
        UsageFlags       usages = UsageFlags::SRV;

        inline constexpr bool operator==(const Desc&) const noexcept;
    };

    inline const auto& GetDesc() const noexcept { return m_Desc; }

protected:
    Texture(Device& device, Desc desc);
    Desc m_Desc;
};

class Sampler : public Resource {
public:
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

        inline constexpr bool operator==(const Desc&) const noexcept;
    };

    inline const auto& GetDesc() const noexcept { return m_Desc; }

protected:
    Sampler(Device& device, Desc desc);
    Desc m_Desc;
};

class SwapChain : public Resource {
public:
    struct Desc {
        std::string_view name = UNKOWN_NAME;
        utils::Window    window;
        Format           format       = Format::B8G8R8A8_UNORM;
        std::uint32_t    sample_count = 1;
        bool             vsync        = false;
    };

    inline const auto& GetDesc() const noexcept { return m_Desc; }

    virtual auto GetCurrentBackBuffer() -> Texture&                                = 0;
    virtual auto GetBuffers() -> std::pmr::vector<std::reference_wrapper<Texture>> = 0;
    virtual auto Width() -> std::uint32_t                                          = 0;
    virtual auto Height() -> std::uint32_t                                         = 0;
    virtual void Present()                                                         = 0;
    virtual void Resize()                                                          = 0;

protected:
    SwapChain(Device& device, Desc desc);

    Desc m_Desc;
};

class GraphicsPipeline : public Resource {
public:
    struct Desc {
        std::string_view name = UNKOWN_NAME;
        // Shader config
        Shader                vs;  // vertex shader
        Shader                ps;  // pixel shader
        Shader                gs;  // geometry shader
        PrimitiveTopology     topology = PrimitiveTopology::TriangleList;
        InputLayout           input_layout;
        RasterizerDescription rasterizer_config;
        BlendDescription      blend_config;
        Format                render_format        = Format::R8G8B8A8_UNORM;
        Format                depth_stencil_format = Format::UNKNOWN;
    };

    inline const auto& GetDesc() const noexcept { return m_Desc; }

protected:
    GraphicsPipeline(Device& device, Desc desc);

    Desc m_Desc;
};

class ComputePipeline : public Resource {
public:
    struct Desc {
        std::string_view name = UNKOWN_NAME;
        Shader           cs;  // computer shader
    };

    inline const auto& GetDesc() const noexcept { return m_Desc; }

protected:
    ComputePipeline(Device& device, Desc desc);

    Desc m_Desc;
};

inline constexpr bool GpuBuffer::Desc::operator==(const Desc& rhs) const noexcept {
    // clang-format off
    return 
        name          == rhs.name          &&
        element_size  == rhs.element_size  &&
        element_count == rhs.element_count &&
        usages        == rhs.usages;
    // clang-format on
}

inline constexpr bool Texture::Desc::operator==(const Desc& rhs) const noexcept {
    // clang-format off
    return 
        name                == rhs.name                &&
        width               == rhs.width               &&
        height              == rhs.height              &&
        depth               == rhs.depth               &&
        array_size          == rhs.array_size          &&
        format              == rhs.format              &&
        mip_levels          == rhs.mip_levels          &&
        sample_count        == rhs.sample_count        &&
        is_cube             == rhs.is_cube             &&
        clear_value.color   == rhs.clear_value.color   &&
        clear_value.depth   == rhs.clear_value.depth   &&
        clear_value.stencil == rhs.clear_value.stencil &&
        usages              == rhs.usages;
    // clang-format on
}

inline constexpr bool Sampler::Desc::operator==(const Desc& rhs) const noexcept {
    // clang-format off
    return
        name           == rhs.name           &&           
        address_u      == rhs.address_u      &&      
        address_v      == rhs.address_v      &&      
        address_w      == rhs.address_w      &&      
        mag_filter     == rhs.mag_filter     &&     
        min_filter     == rhs.min_filter     &&     
        mipmap_filter  == rhs.mipmap_filter  &&  
        min_lod        == rhs.min_lod        &&        
        max_load       == rhs.max_load       &&       
        max_anisotropy == rhs.max_anisotropy && 
        compare        == rhs.compare;
    // clang-format on
}

}  // namespace hitagi::gfx

template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::GpuBuffer::UsageFlags> {
    static constexpr bool is_flags = true;
};
template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::Texture::UsageFlags> {
    static constexpr bool is_flags = true;
};

namespace std {
template <>
struct hash<hitagi::gfx::GpuBuffer::Desc> {
    constexpr std::size_t operator()(const hitagi::gfx::GpuBuffer::Desc& desc) const noexcept {
        return hitagi::utils::combine_hash(std::array{
            hitagi::utils::hash(desc.name),
            hitagi::utils::hash(desc.element_size),
            hitagi::utils::hash(desc.element_count),
            hitagi::utils::hash(desc.usages),
        });
    }
};

template <>
struct hash<hitagi::gfx::Texture::Desc> {
    constexpr std::size_t operator()(const hitagi::gfx::Texture::Desc& desc) const noexcept {
        return hitagi::utils::combine_hash(std::array{
            hitagi::utils::hash(desc.name),
            hitagi::utils::hash(desc.width),
            hitagi::utils::hash(desc.height),
            hitagi::utils::hash(desc.depth),
            hitagi::utils::hash(desc.array_size),
            hitagi::utils::hash(desc.format),
            hitagi::utils::hash(desc.mip_levels),
            hitagi::utils::hash(desc.sample_count),
            hitagi::utils::hash(desc.is_cube),
            hitagi::utils::hash(desc.usages),
        });
    }
};

template <>
struct hash<hitagi::gfx::Sampler::Desc> {
    constexpr std::size_t operator()(const hitagi::gfx::Sampler::Desc& desc) const noexcept {
        return hitagi::utils::combine_hash(std::array{
            hitagi::utils::hash(desc.name),
            hitagi::utils::hash(desc.address_u),
            hitagi::utils::hash(desc.address_v),
            hitagi::utils::hash(desc.address_w),
            hitagi::utils::hash(desc.mag_filter),
            hitagi::utils::hash(desc.min_filter),
            hitagi::utils::hash(desc.mipmap_filter),
            hitagi::utils::hash(desc.min_lod),
            hitagi::utils::hash(desc.max_load),
            hitagi::utils::hash(desc.max_anisotropy),
            hitagi::utils::hash(desc.compare),
        });
    }
};

}  // namespace std