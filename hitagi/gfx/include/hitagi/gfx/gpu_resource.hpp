#pragma once
#include <hitagi/gfx/common_types.hpp>
#include <hitagi/core/buffer.hpp>
#include <hitagi/utils/types.hpp>
#include <hitagi/utils/hash.hpp>

#include <filesystem>

namespace hitagi::gfx {
class Device;
class Fence;

constexpr auto UNKOWN_NAME = "Unkown";

template <typename Desc>
class Resource {
public:
    Resource(const Resource&)                = delete;
    Resource(Resource&&) noexcept            = default;
    Resource& operator=(const Resource&)     = delete;
    Resource& operator=(Resource&&) noexcept = delete;
    virtual ~Resource()                      = default;

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
    std::string_view          name         = UNKOWN_NAME;
    std::uint32_t             width        = 1;
    std::uint32_t             height       = 1;
    std::uint16_t             depth        = 1;
    std::uint16_t             array_size   = 1;
    Format                    format       = Format::UNKNOWN;
    std::uint16_t             mip_levels   = 1;
    std::uint32_t             sample_count = 1;
    std::optional<ClearValue> clear_value  = std::nullopt;
    TextureUsageFlags         usages       = TextureUsageFlags::SRV;

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
    float            max_lod        = 32;
    float            max_anisotropy = 1;
    CompareOp        compare_op;

    inline constexpr bool operator==(const SamplerDesc&) const noexcept;
};
using Sampler = Resource<SamplerDesc>;

struct SwapChainDesc {
    std::string_view          name = UNKOWN_NAME;
    utils::Window             window;
    std::optional<ClearValue> clear_value  = std::nullopt;
    std::uint32_t             sample_count = 1;
    bool                      vsync        = false;
};
class SwapChain : public Resource<SwapChainDesc> {
public:
    virtual auto GetTexture(std::uint32_t index) -> Texture&                        = 0;
    virtual auto GetTextures() -> std::pmr::vector<std::reference_wrapper<Texture>> = 0;
    virtual auto GetWidth() const noexcept -> std::uint32_t                         = 0;
    virtual auto GetHeight() const noexcept -> std::uint32_t                        = 0;
    virtual auto GetFormat() const noexcept -> Format                               = 0;
    virtual void Present()                                                          = 0;
    virtual void Resize()                                                           = 0;

protected:
    using Resource::Resource;
};

struct ShaderDesc {
    std::string_view      name;
    ShaderType            type;
    std::string_view      entry;
    std::string_view      source_code;
    std::filesystem::path path;
};

class Shader : public Resource<ShaderDesc> {
public:
    virtual auto GetDXILData() const noexcept -> std::span<const std::byte>;
    virtual auto GetSPIRVData() const noexcept -> std::span<const std::byte>;

protected:
    using Resource::Resource;
};

struct RenderPipelineDesc {
    std::string_view name = UNKOWN_NAME;

    std::pmr::vector<std::weak_ptr<Shader>> shaders;

    AssemblyState      assembly_state       = {};
    VertexLayout       vertex_input_layout  = {};
    RasterizationState rasterization_state  = {};
    DepthStencilState  depth_stencil_state  = {};
    BlendState         blend_state          = {};
    Format             render_format        = Format::R8G8B8A8_UNORM;
    Format             depth_stencil_format = Format::UNKNOWN;
};
using RenderPipeline = Resource<RenderPipelineDesc>;

struct ComputePipelineDesc {
    std::string_view name = UNKOWN_NAME;

    std::weak_ptr<Shader> cs;  // computer shader
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
    bool result = 
        name                    == rhs.name                    &&
        width                   == rhs.width                   &&
        height                  == rhs.height                  &&
        depth                   == rhs.depth                   &&
        array_size              == rhs.array_size              &&
        format                  == rhs.format                  &&
        mip_levels              == rhs.mip_levels              &&
        sample_count            == rhs.sample_count            &&
        clear_value.has_value() == rhs.clear_value.has_value() &&
        usages                  == rhs.usages;
    // clang-format on
    if (clear_value.has_value() && rhs.clear_value.has_value()) {
        if (std::holds_alternative<ClearColor>(clear_value.value()) && std::holds_alternative<ClearColor>(rhs.clear_value.value())) {
            result = result && (std::get<ClearColor>(clear_value.value()) == std::get<ClearColor>(rhs.clear_value.value()));
        }
        if (std::holds_alternative<ClearDepthStencil>(clear_value.value()) && std::holds_alternative<ClearDepthStencil>(rhs.clear_value.value())) {
            result = result &&
                     (std::get<ClearDepthStencil>(clear_value.value()).depth == std::get<ClearDepthStencil>(rhs.clear_value.value()).depth) &&
                     (std::get<ClearDepthStencil>(clear_value.value()).stencil == std::get<ClearDepthStencil>(rhs.clear_value.value()).stencil);
        }
    }

    return result;
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
        max_lod        == rhs.max_lod        &&       
        max_anisotropy == rhs.max_anisotropy && 
        compare_op     == rhs.compare_op;
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
            hitagi::utils::hash(desc.max_lod),
            hitagi::utils::hash(desc.max_anisotropy),
            hitagi::utils::hash(desc.compare_op),
        });
    }
};

}  // namespace std