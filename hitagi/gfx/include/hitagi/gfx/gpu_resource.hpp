#pragma once
#include <hitagi/gfx/common_types.hpp>
#include <hitagi/gfx/sync.hpp>
#include <hitagi/core/buffer.hpp>
#include <hitagi/utils/types.hpp>
#include <hitagi/utils/hash.hpp>

#include <functional>

namespace hitagi::gfx {
class Device;

constexpr auto UNKOWN_NAME = "Unkown";

template <typename Desc>
class Resource {
public:
    Resource(const Resource&) = delete;
    virtual ~Resource()       = default;

    inline auto  GetName() const noexcept -> std::string_view { return m_Desc.name; }
    inline auto& GetDevice() const noexcept { return m_Device; }
    inline auto& GetDesc() const noexcept { return m_Desc; }

protected:
    Resource(Device& device, Desc desc) : m_Device(device), m_Name(desc.name), m_Desc(std::move(desc)) { m_Desc.name = m_Name; }

    Device&          m_Device;
    std::pmr::string m_Name;
    Desc             m_Desc;
};

struct GPUBufferDesc {
    std::string_view    name = UNKOWN_NAME;
    std::uint64_t       element_size;
    std::uint64_t       element_count = 1;
    GPUBufferUsageFlags usages;

    inline constexpr bool operator==(const GPUBufferDesc&) const noexcept;
};

class GPUBuffer : public Resource<GPUBufferDesc> {
public:
    template <typename T>
    void Update(std::size_t index, T data) {
        UpdateRaw(index, std::span(reinterpret_cast<const std::byte*>(&data), sizeof(T)));
    }
    virtual auto GetMappedPtr() const noexcept -> std::byte* = 0;

protected:
    using Resource::Resource;
    void UpdateRaw(std::size_t index, std::span<const std::byte> data);
};

struct TextureDesc {
    std::string_view  name         = UNKOWN_NAME;
    std::uint32_t     width        = 1;
    std::uint32_t     height       = 1;
    std::uint16_t     depth        = 1;
    std::uint16_t     array_size   = 1;
    Format            format       = Format::UNKNOWN;
    std::uint16_t     mip_levels   = 1;
    std::uint32_t     sample_count = 1;
    bool              is_cube      = false;
    ClearValue        clear_value;
    TextureUsageFlags usages = TextureUsageFlags::SRV;

    inline constexpr bool operator==(const TextureDesc&) const noexcept;
};
using Texture = Resource<TextureDesc>;

struct SamplerDesc {
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
    CompareOp        compare;

    inline constexpr bool operator==(const SamplerDesc&) const noexcept;
};
using Sampler = Resource<SamplerDesc>;

struct SwapChainDesc {
    std::string_view name = UNKOWN_NAME;
    utils::Window    window;
    Format           format       = Format::B8G8R8A8_UNORM;
    std::uint32_t    sample_count = 1;
    bool             vsync        = false;
};
class SwapChain : public Resource<SwapChainDesc> {
public:
    virtual auto GetCurrentBackBuffer() -> Texture&                                = 0;
    virtual auto GetBuffers() -> std::pmr::vector<std::reference_wrapper<Texture>> = 0;
    virtual auto Width() -> std::uint32_t                                          = 0;
    virtual auto Height() -> std::uint32_t                                         = 0;
    virtual void Present()                                                         = 0;
    virtual void Resize()                                                          = 0;

protected:
    using Resource::Resource;
};

struct ShaderDesc {
    std::string_view name;
    ShaderType       type;
    std::string_view entry;
    std::string_view source_code;
};

class Shader : public Resource<ShaderDesc> {
public:
    virtual auto GetDXILData() const noexcept -> std::span<const std::byte>;
    virtual auto GetSPIRVData() const noexcept -> std::span<const std::byte>;

protected:
    using Resource::Resource;
};

struct RootSignatureDesc {
    std::string_view name;
};
using RootSignature = Resource<RootSignatureDesc>;

struct GraphicsPipelineDesc {
    std::string_view name = UNKOWN_NAME;
    // Shader config
    std::shared_ptr<Shader> vs;  // vertex shader
    std::shared_ptr<Shader> ps;  // pixel shader
    std::shared_ptr<Shader> gs;  // geometry shader
    AssemblyState           assembly_state = {};
    VertexBufferLayouts     vertex_input_layout;
    RasterizationState      rasterization_state;
    DepthStencilState       depth_stencil_state;
    BlendState              blend_state;
    Format                  render_format        = Format::R8G8B8A8_UNORM;
    Format                  depth_stencil_format = Format::UNKNOWN;
};
using GraphicsPipeline = Resource<GraphicsPipelineDesc>;

struct ComputePipelineDesc {
    std::string_view        name = UNKOWN_NAME;
    std::shared_ptr<Shader> cs;  // computer shader
};
using ComputePipeline = Resource<ComputePipelineDesc>;

inline constexpr bool GPUBufferDesc::operator==(const GPUBufferDesc& rhs) const noexcept {
    // clang-format off
    return 
        name          == rhs.name          &&
        element_size  == rhs.element_size  &&
        element_count == rhs.element_count &&
        usages        == rhs.usages;
    // clang-format on
}

inline constexpr bool TextureDesc::operator==(const TextureDesc& rhs) const noexcept {
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

inline constexpr bool SamplerDesc::operator==(const SamplerDesc& rhs) const noexcept {
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

namespace std {
template <>
struct hash<hitagi::gfx::GPUBufferDesc> {
    constexpr std::size_t operator()(const hitagi::gfx::GPUBufferDesc& desc) const noexcept {
        return hitagi::utils::combine_hash(std::array{
            hitagi::utils::hash(desc.name),
            hitagi::utils::hash(desc.element_size),
            hitagi::utils::hash(desc.element_count),
            hitagi::utils::hash(desc.usages),
        });
    }
};

template <>
struct hash<hitagi::gfx::TextureDesc> {
    constexpr std::size_t operator()(const hitagi::gfx::TextureDesc& desc) const noexcept {
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
struct hash<hitagi::gfx::SamplerDesc> {
    constexpr std::size_t operator()(const hitagi::gfx::SamplerDesc& desc) const noexcept {
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