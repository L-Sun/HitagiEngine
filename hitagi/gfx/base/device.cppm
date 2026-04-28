module;
#if defined(_WIN32)
#include <unknwn.h>
#include <dxc/dxcapi.h>
#endif
#include <spdlog/logger.h>

export module gfx.base;
import std;
import utils;
import math;
import core;
import magic_enum;

export namespace hitagi::gfx {

enum struct ResourceType : std::uint8_t {
    GPUBuffer,
    Texture,
    Sampler,
    SwapChain,
    Shader,
    RenderPipeline,
    ComputePipeline,
};

enum struct GPUBufferUsageFlags : std::uint8_t {
    MapRead  = 0x1,
    MapWrite = (MapRead << 1),
    CopySrc  = (MapWrite << 1),
    CopyDst  = (CopySrc << 1),
    Vertex   = (CopyDst << 1),
    Index    = (Vertex << 1),
    Constant = (Index << 1),
    Storage  = (Constant << 1),
};

enum struct TextureUsageFlags : std::uint8_t {
    CopySrc      = 0x1,
    CopyDst      = (CopySrc << 1),
    SRV          = (CopyDst << 1),
    UAV          = (SRV << 1),
    RenderTarget = (UAV << 1),
    DepthStencil = (RenderTarget << 1),
    Cube         = (DepthStencil << 1),
    CubeArray    = (Cube << 1),
};

enum struct ShaderType : std::uint8_t {
    Vertex,
    Pixel,
    Geometry,
    Compute,
};

enum struct Format : std::uint32_t {
    UNKNOWN                    = 0,
    R32G32B32A32_TYPELESS      = 1,
    R32G32B32A32_FLOAT         = 2,
    R32G32B32A32_UINT          = 3,
    R32G32B32A32_SINT          = 4,
    R32G32B32_TYPELESS         = 5,
    R32G32B32_FLOAT            = 6,
    R32G32B32_UINT             = 7,
    R32G32B32_SINT             = 8,
    R16G16B16A16_TYPELESS      = 9,
    R16G16B16A16_FLOAT         = 10,
    R16G16B16A16_UNORM         = 11,
    R16G16B16A16_UINT          = 12,
    R16G16B16A16_SNORM         = 13,
    R16G16B16A16_SINT          = 14,
    R32G32_TYPELESS            = 15,
    R32G32_FLOAT               = 16,
    R32G32_UINT                = 17,
    R32G32_SINT                = 18,
    R32G8X24_TYPELESS          = 19,
    D32_FLOAT_S8X24_UINT       = 20,
    R32_FLOAT_X8X24_TYPELESS   = 21,
    X32_TYPELESS_G8X24_UINT    = 22,
    R10G10B10A2_TYPELESS       = 23,
    R10G10B10A2_UNORM          = 24,
    R10G10B10A2_UINT           = 25,
    R11G11B10_FLOAT            = 26,
    R8G8B8A8_TYPELESS          = 27,
    R8G8B8A8_UNORM             = 28,
    R8G8B8A8_UNORM_SRGB        = 29,
    R8G8B8A8_UINT              = 30,
    R8G8B8A8_SNORM             = 31,
    R8G8B8A8_SINT              = 32,
    R16G16_TYPELESS            = 33,
    R16G16_FLOAT               = 34,
    R16G16_UNORM               = 35,
    R16G16_UINT                = 36,
    R16G16_SNORM               = 37,
    R16G16_SINT                = 38,
    R32_TYPELESS               = 39,
    D32_FLOAT                  = 40,
    R32_FLOAT                  = 41,
    R32_UINT                   = 42,
    R32_SINT                   = 43,
    R24G8_TYPELESS             = 44,
    D24_UNORM_S8_UINT          = 45,
    R24_UNORM_X8_TYPELESS      = 46,
    X24_TYPELESS_G8_UINT       = 47,
    R8G8_TYPELESS              = 48,
    R8G8_UNORM                 = 49,
    R8G8_UINT                  = 50,
    R8G8_SNORM                 = 51,
    R8G8_SINT                  = 52,
    R16_TYPELESS               = 53,
    R16_FLOAT                  = 54,
    D16_UNORM                  = 55,
    R16_UNORM                  = 56,
    R16_UINT                   = 57,
    R16_SNORM                  = 58,
    R16_SINT                   = 59,
    R8_TYPELESS                = 60,
    R8_UNORM                   = 61,
    R8_UINT                    = 62,
    R8_SNORM                   = 63,
    R8_SINT                    = 64,
    A8_UNORM                   = 65,
    R1_UNORM                   = 66,
    R9G9B9E5_SHAREDEXP         = 67,
    R8G8_B8G8_UNORM            = 68,
    G8R8_G8B8_UNORM            = 69,
    BC1_TYPELESS               = 70,
    BC1_UNORM                  = 71,
    BC1_UNORM_SRGB             = 72,
    BC2_TYPELESS               = 73,
    BC2_UNORM                  = 74,
    BC2_UNORM_SRGB             = 75,
    BC3_TYPELESS               = 76,
    BC3_UNORM                  = 77,
    BC3_UNORM_SRGB             = 78,
    BC4_TYPELESS               = 79,
    BC4_UNORM                  = 80,
    BC4_SNORM                  = 81,
    BC5_TYPELESS               = 82,
    BC5_UNORM                  = 83,
    BC5_SNORM                  = 84,
    B5G6R5_UNORM               = 85,
    B5G5R5A1_UNORM             = 86,
    B8G8R8A8_UNORM             = 87,
    B8G8R8X8_UNORM             = 88,
    R10G10B10_XR_BIAS_A2_UNORM = 89,
    B8G8R8A8_TYPELESS          = 90,
    B8G8R8A8_UNORM_SRGB        = 91,
    B8G8R8X8_TYPELESS          = 92,
    B8G8R8X8_UNORM_SRGB        = 93,
    BC6H_TYPELESS              = 94,
    BC6H_UF16                  = 95,
    BC6H_SF16                  = 96,
    BC7_TYPELESS               = 97,
    BC7_UNORM                  = 98,
    BC7_UNORM_SRGB             = 99,
};

enum struct PrimitiveTopology : std::uint8_t {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    Unkown,
};

enum struct FillMode : std::uint8_t {
    Solid,
    Wireframe,
};

enum struct CullMode : std::uint8_t {
    None,
    Front,
    Back
};

enum struct FrontFace : std::uint8_t {
    CW,
    CCW,
};

enum struct ColorMask : std::uint8_t {
    R   = 1,
    G   = (R << 1),
    B   = (G << 1),
    A   = (B << 1),
    All = R | G | B | A,
};

enum struct StencilOp {
    Keep,
    Zero,
    Replace,
    Invert,
    IncrementClamp,
    DecrementClamp,
    IncrementWrap,
    DecrementWrap,
};

enum struct BlendFactor : std::uint8_t {
    Zero,
    One,
    SrcColor,
    InvSrcColor,
    SrcAlpha,
    InvSrcAlpha,
    DstColor,
    InvDstColor,
    DstAlpha,
    InvDstAlpha,
    Constant,
    InvConstant,
    SrcAlphaSat,
};

enum struct BlendOp : std::uint8_t {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max
};

enum struct LogicOp : std::uint8_t {
    Clear,
    Set,
    Copy,
    CopyInverted,
    NoOp,
    Invert,
    And,
    Nand,
    Or,
    Nor,
    Xor,
    Equiv,
    AndReverse,
    AndInverted,
    OrReverse,
    OrInverted,
};

enum struct CompareOp : std::uint8_t {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

struct RasterizationState {
    FillMode fill_mode               = FillMode::Solid;
    CullMode cull_mode               = CullMode::None;
    bool     front_counter_clockwise = true;

    bool  depth_clamp_enable      = false;
    bool  depth_bias_enable       = false;
    float depth_bias              = 0;
    float depth_bias_clamp        = 0.0f;
    float depth_bias_slope_factor = 0.0f;
};

struct StencilOpState {
    StencilOp     fail_op       = StencilOp::Keep;
    StencilOp     pass_op       = StencilOp::Keep;
    StencilOp     depth_fail_op = StencilOp::Keep;
    CompareOp     compare_op    = CompareOp::Always;
    std::uint32_t compare_mask  = 0xFFFFFFFF;
    std::uint32_t write_mask    = 0xFFFFFFFF;
    std::uint32_t reference     = 0xFFFFFFFF;
};

struct DepthStencilState {
    bool      depth_test_enable  = false;
    bool      depth_write_enable = false;
    CompareOp depth_compare_op   = CompareOp::Less;

    bool           stencil_test_enable = false;
    StencilOpState front               = {};
    StencilOpState back                = {};

    bool        depth_bounds_test_enable = false;
    math::vec2f depth_bounds             = math::vec2f(0.0f, 1.0f);
};

struct BlendState {
    bool        blend_enable           = false;
    BlendFactor src_color_blend_factor = BlendFactor::One;
    BlendFactor dst_color_blend_factor = BlendFactor::Zero;
    BlendOp     color_blend_op         = BlendOp::Add;
    BlendFactor src_alpha_blend_factor = BlendFactor::One;
    BlendFactor dst_alpha_blend_factor = BlendFactor::Zero;
    BlendOp     alpha_blend_op         = BlendOp::Add;
    math::vec4f blend_constants        = math::vec4f(1.0f);
    ColorMask   color_write_mask       = ColorMask::All;

    bool    logic_operation_enable = false;
    LogicOp logic_op               = LogicOp::NoOp;
};

using ClearColor = math::Color;
struct ClearDepthStencil {
    float         depth   = 1.0f;
    std::uint32_t stencil = 0;
};
using ClearValue = std::variant<ClearColor, ClearDepthStencil>;

struct AssemblyState {
    PrimitiveTopology primitive      = PrimitiveTopology::TriangleList;
    bool              restart_enable = false;
};

struct VertexAttribute {
    std::pmr::string semantic;
    Format           format;
    std::uint32_t    binding;
    std::uint64_t    offset       = 0;
    std::uint64_t    stride       = 0;
    bool             per_instance = false;
};
using VertexLayout = std::pmr::vector<VertexAttribute>;

enum struct AddressMode : std::uint8_t {
    Clamp,
    Repeat,
    MirrorRepeat
};

enum struct FilterMode : std::uint8_t {
    Point,
    Linear
};

struct ViewPort {
    float x, y;
    float width, height;
    float min_depth = 0.0f;
    float max_depth = 1.0f;
};

struct Rect {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t width;
    std::uint32_t height;
};

struct TextureSubresourceLayer {
    std::uint32_t mip_level        = 0;
    std::uint32_t base_array_layer = 0;
    std::uint32_t layer_count      = 1;

    bool operator==(const TextureSubresourceLayer& rhs) const noexcept {
        return mip_level == rhs.mip_level &&
               base_array_layer == rhs.base_array_layer &&
               layer_count == rhs.layer_count;
    }
};

enum struct BarrierAccess : std::uint32_t {
    None              = 0x1,
    CopySrc           = (None << 1),
    CopyDst           = (CopySrc << 1),
    Vertex            = (CopyDst << 1),
    Index             = (Vertex << 1),
    Constant          = (Index << 1),
    ShaderRead        = (Constant << 1),
    ShaderWrite       = (ShaderRead << 1),
    DepthStencilRead  = (ShaderWrite << 1),
    DepthStencilWrite = (DepthStencilRead << 1),
    RenderTarget      = (DepthStencilWrite << 1),
    Present           = (RenderTarget << 1),
};

enum struct PipelineStage : std::uint32_t {
    None          = 0x1,
    VertexInput   = (None << 1),
    VertexShader  = (VertexInput << 1),
    PixelShader   = (VertexShader << 1),
    DepthStencil  = (PixelShader << 1),
    Render        = (DepthStencil << 1),
    Resolve       = (Render << 1),
    AllGraphics   = (Resolve << 1),
    ComputeShader = (AllGraphics << 1),
    Copy          = (ComputeShader << 1),
    All           = (Copy << 1),
};
inline constexpr bool operator<(PipelineStage lhs, PipelineStage rhs) noexcept {
    return std::countl_zero(static_cast<std::underlying_type_t<PipelineStage>>(lhs)) > std::countl_zero(static_cast<std::underlying_type_t<PipelineStage>>(rhs));
}

enum struct TextureLayout : std::uint16_t {
    Unkown,
    Common,
    CopySrc,
    CopyDst,
    ShaderRead,
    ShaderWrite,
    DepthStencilRead,
    DepthStencilWrite,
    RenderTarget,
    ResolveSrc,
    ResolveDst,
    Present,
};

}  // namespace hitagi::gfx

export template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::GPUBufferUsageFlags> {
    static constexpr bool is_flags = true;
};
export template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::TextureUsageFlags> {
    static constexpr bool is_flags = true;
};
export template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::BarrierAccess> {
    static constexpr bool is_flags = true;
};
export template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::PipelineStage> {
    static constexpr bool is_flags = true;
};

export namespace hitagi::gfx {

class Device;
class Fence;
struct GPUBufferBarrier;
struct TextureBarrier;

constexpr auto UNKOWN_NAME = "Unkown";

class Resource {
public:
    Resource(const Resource&)                = delete;
    Resource(Resource&&) noexcept            = default;
    Resource& operator=(const Resource&)     = delete;
    Resource& operator=(Resource&&) noexcept = delete;
    virtual ~Resource()                      = default;

    inline auto& GetDevice() const noexcept { return m_Device; }

    virtual auto GetName() const noexcept -> std::string_view = 0;
    virtual auto GetType() const noexcept -> ResourceType     = 0;

protected:
    Resource(Device& device) : m_Device(device) {}

    Device& m_Device;
};

template <typename T>
concept ResourceDesc = requires(T t) {
    { T::name } -> std::convertible_to<std::string_view>;
};

template <ResourceDesc _Desc>
class ResourceWithDesc : public Resource {
public:
    using Desc = _Desc;

    auto         GetName() const noexcept -> std::string_view final { return m_Desc.name; };
    auto         GetType() const noexcept -> ResourceType final;
    inline auto& GetDesc() const noexcept { return m_Desc; }

protected:
    ResourceWithDesc(Device& device, _Desc desc) : Resource(device), m_Desc(std::move(desc)) {}

    _Desc m_Desc;
};

struct GPUBufferDesc {
    std::pmr::string    name = UNKOWN_NAME;
    std::uint64_t       element_size;
    std::uint64_t       element_count = 1;
    GPUBufferUsageFlags usages;

    inline constexpr bool operator==(const GPUBufferDesc&) const noexcept;
};

class GPUBuffer : public ResourceWithDesc<GPUBufferDesc> {
public:
    struct Pointer;

    inline auto AlignedElementSize() const noexcept -> std::uint64_t { return utils::align(m_Desc.element_size, m_ElementAlignment); }
    inline auto Size() const noexcept -> std::uint64_t { return AlignedElementSize() * m_Desc.element_count; }

    virtual auto Map() -> std::byte* = 0;
    virtual void UnMap()             = 0;

    [[nodiscard]] auto Transition(BarrierAccess access, PipelineStage stage = PipelineStage::All) -> GPUBufferBarrier;

protected:
    using ResourceWithDesc::ResourceWithDesc;

    template <typename T>
        requires(!std::is_reference_v<T>)
    friend struct GPUBufferView;
    friend struct GPUBufferPointer;

    std::uint64_t m_ElementAlignment = 1;
    BarrierAccess m_CurrentAccess    = BarrierAccess::None;
    PipelineStage m_CurrentStage     = PipelineStage::None;
};

template <typename T>
    requires(!std::is_reference_v<T>)
struct GPUBufferView : public utils::AlignedSpan<T> {
    GPUBufferView(GPUBuffer& buffer)
        : utils::AlignedSpan<T>(nullptr, buffer.m_Desc.element_count, buffer.m_ElementAlignment),
          buffer(buffer) {
        if (!utils::has_flag(buffer.m_Desc.usages, GPUBufferUsageFlags::MapRead) &&
            !utils::has_flag(buffer.m_Desc.usages, GPUBufferUsageFlags::MapWrite))
            throw std::invalid_argument(std::format("GPUBuffer {} is not mappable", buffer.GetName()));

        if (!std::is_const_v<T> && !utils::has_flag(buffer.m_Desc.usages, GPUBufferUsageFlags::MapWrite))
            throw std::invalid_argument(std::format("GPUBuffer {} is not writable on host", buffer.GetName()));

        this->m_Data = buffer.Map();
    }

    ~GPUBufferView() { buffer.UnMap(); }

    GPUBuffer& buffer;
};

struct TextureDesc {
    std::pmr::string          name        = UNKOWN_NAME;
    std::uint32_t             width       = 1;
    std::uint32_t             height      = 1;
    std::uint16_t             depth       = 1;
    std::uint16_t             array_size  = 1;
    Format                    format      = Format::UNKNOWN;
    std::uint16_t             mip_levels  = 1;
    std::optional<ClearValue> clear_value = std::nullopt;
    TextureUsageFlags         usages      = TextureUsageFlags::SRV;

    inline constexpr bool operator==(const TextureDesc&) const noexcept;
};

class Texture : public ResourceWithDesc<TextureDesc> {
public:
    [[nodiscard]] auto Transition(BarrierAccess access, TextureLayout layout, PipelineStage stage = PipelineStage::All) -> TextureBarrier;

    inline auto GetCurrentLayout() const noexcept -> TextureLayout { return m_CurrentLayout; }

protected:
    using ResourceWithDesc::ResourceWithDesc;

    BarrierAccess m_CurrentAccess = BarrierAccess::None;
    PipelineStage m_CurrentStage  = PipelineStage::All;
    TextureLayout m_CurrentLayout = TextureLayout::Unkown;
};

struct SamplerDesc {
    std::pmr::string name           = UNKOWN_NAME;
    AddressMode      address_u      = AddressMode::Clamp;
    AddressMode      address_v      = AddressMode::Clamp;
    AddressMode      address_w      = AddressMode::Clamp;
    FilterMode       mag_filter     = FilterMode::Point;
    FilterMode       min_filter     = FilterMode::Point;
    FilterMode       mipmap_filter  = FilterMode::Point;
    float            min_lod        = 0;
    float            max_lod        = 32;
    float            max_anisotropy = 1;
    CompareOp        compare_op     = CompareOp::Never;

    inline constexpr bool operator==(const SamplerDesc&) const noexcept;
};
using Sampler = ResourceWithDesc<SamplerDesc>;

struct SwapChainDesc {
    std::pmr::string name = UNKOWN_NAME;
    utils::Window    window;
    ClearColor       clear_color  = math::Color(0, 0, 0, 1);
    std::uint32_t    sample_count = 1;
    bool             vsync        = false;
};
class SwapChain : public ResourceWithDesc<SwapChainDesc> {
public:
    virtual auto AcquireTextureForRendering() -> utils::optional_ref<Texture> = 0;
    virtual auto GetWidth() const noexcept -> std::uint32_t                   = 0;
    virtual auto GetHeight() const noexcept -> std::uint32_t                  = 0;
    virtual auto GetFormat() const noexcept -> Format                         = 0;
    virtual void Present()                                                    = 0;
    virtual void Resize()                                                     = 0;

protected:
    using ResourceWithDesc::ResourceWithDesc;
};

struct ShaderDesc {
    std::pmr::string      name;
    ShaderType            type;
    std::pmr::string      entry = "main";
    std::pmr::string      source_code;
    std::filesystem::path path;
};

class Shader : public ResourceWithDesc<ShaderDesc> {
public:
    virtual auto GetDXILData() const noexcept -> std::span<const std::byte> { return {}; }
    virtual auto GetSPIRVData() const noexcept -> std::span<const std::byte> { return {}; }

protected:
    using ResourceWithDesc::ResourceWithDesc;
};

struct RenderPipelineDesc {
    std::pmr::string name = UNKOWN_NAME;

    std::pmr::vector<std::weak_ptr<Shader>> shaders;

    AssemblyState      assembly_state       = {};
    VertexLayout       vertex_input_layout  = {};
    RasterizationState rasterization_state  = {};
    DepthStencilState  depth_stencil_state  = {};
    BlendState         blend_state          = {};
    Format             render_format        = Format::R8G8B8A8_UNORM;
    Format             depth_stencil_format = Format::UNKNOWN;
};
using RenderPipeline = ResourceWithDesc<RenderPipelineDesc>;

struct ComputePipelineDesc {
    std::pmr::string name = UNKOWN_NAME;

    std::weak_ptr<Shader> cs;
};
using ComputePipeline = ResourceWithDesc<ComputePipelineDesc>;

template <ResourceDesc Desc>
auto ResourceWithDesc<Desc>::GetType() const noexcept -> ResourceType {
    if constexpr (std::is_same_v<Desc, GPUBufferDesc>) {
        return ResourceType::GPUBuffer;
    } else if constexpr (std::is_same_v<Desc, TextureDesc>) {
        return ResourceType::Texture;
    } else if constexpr (std::is_same_v<Desc, SamplerDesc>) {
        return ResourceType::Sampler;
    } else if constexpr (std::is_same_v<Desc, SwapChainDesc>) {
        return ResourceType::SwapChain;
    } else if constexpr (std::is_same_v<Desc, ShaderDesc>) {
        return ResourceType::Shader;
    } else if constexpr (std::is_same_v<Desc, RenderPipelineDesc>) {
        return ResourceType::RenderPipeline;
    } else if constexpr (std::is_same_v<Desc, ComputePipelineDesc>) {
        return ResourceType::ComputePipeline;
    } else {
        []<bool flag = false>() { static_assert(flag, "Unknown resource type"); }();
    }
}

inline constexpr bool GPUBufferDesc::operator==(const GPUBufferDesc& rhs) const noexcept {
    return name == rhs.name &&
           element_size == rhs.element_size &&
           element_count == rhs.element_count &&
           usages == rhs.usages;
}

inline constexpr bool TextureDesc::operator==(const TextureDesc& rhs) const noexcept {
    bool result =
        name == rhs.name &&
        width == rhs.width &&
        height == rhs.height &&
        depth == rhs.depth &&
        array_size == rhs.array_size &&
        format == rhs.format &&
        mip_levels == rhs.mip_levels &&
        clear_value.has_value() == rhs.clear_value.has_value() &&
        usages == rhs.usages;
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
    return name == rhs.name &&
           address_u == rhs.address_u &&
           address_v == rhs.address_v &&
           address_w == rhs.address_w &&
           mag_filter == rhs.mag_filter &&
           min_filter == rhs.min_filter &&
           mipmap_filter == rhs.mipmap_filter &&
           min_lod == rhs.min_lod &&
           max_lod == rhs.max_lod &&
           max_anisotropy == rhs.max_anisotropy &&
           compare_op == rhs.compare_op;
}

}  // namespace hitagi::gfx

export namespace std {
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

export namespace hitagi::gfx {

class Fence {
public:
    Fence(const Fence&)            = delete;
    Fence(Fence&&)                 = default;
    Fence& operator=(const Fence&) = delete;
    Fence& operator=(Fence&&)      = delete;
    virtual ~Fence()               = default;

    virtual void Signal(std::uint64_t value)                                                                       = 0;
    virtual bool Wait(std::uint64_t value, std::chrono::milliseconds timeout = (std::chrono::milliseconds::max)()) = 0;
    virtual auto GetCurrentValue() -> std::uint64_t                                                                = 0;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }

protected:
    Fence(Device& device, std::string_view name = "") : m_Device(device), m_Name(name) {}

    Device&          m_Device;
    std::pmr::string m_Name;
};

struct GlobalBarrier {
    BarrierAccess src_access = BarrierAccess::None;
    BarrierAccess dst_access = BarrierAccess::None;
    PipelineStage src_stage  = PipelineStage::None;
    PipelineStage dst_stage  = PipelineStage::None;
};

struct GPUBufferBarrier {
    BarrierAccess src_access = BarrierAccess::None;
    BarrierAccess dst_access = BarrierAccess::None;
    PipelineStage src_stage  = PipelineStage::None;
    PipelineStage dst_stage  = PipelineStage::None;

    GPUBuffer& buffer;
};

struct TextureBarrier {
    BarrierAccess src_access = BarrierAccess::None;
    BarrierAccess dst_access = BarrierAccess::None;
    PipelineStage src_stage  = PipelineStage::None;
    PipelineStage dst_stage  = PipelineStage::None;

    TextureLayout src_layout = TextureLayout::Unkown;
    TextureLayout dst_layout = TextureLayout::Unkown;

    Texture& texture;
};

struct FenceSignalInfo {
    Fence&        fence;
    std::uint64_t value;
};

struct FenceWaitInfo {
    Fence&        fence;
    std::uint64_t value;
    PipelineStage stage = PipelineStage::All;
};

enum struct BindlessHandleType : std::uint32_t {
    Buffer,
    Texture,
    Sampler,
    Invalid,
};

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

enum struct CommandType : std::uint8_t {
    Graphics,
    Compute,
    Copy
};

class CommandContext {
public:
    virtual ~CommandContext() = default;

    virtual void Begin() = 0;
    virtual void End()   = 0;

    virtual void ResourceBarrier(
        std::span<const GlobalBarrier>    global_barriers  = {},
        std::span<const GPUBufferBarrier> buffer_barriers  = {},
        std::span<const TextureBarrier>   texture_barriers = {}) = 0;

    inline auto& GetDevice() const noexcept { return m_Device; }
    inline auto  GetName() const noexcept -> std::string_view { return m_Name; }
    inline auto  GetType() const noexcept { return m_Type; }

protected:
    CommandContext(Device& device, CommandType type, std::string_view name)
        : m_Device(device), m_Type(type), m_Name(name) {}

    Device&           m_Device;
    const CommandType m_Type;
    std::pmr::string  m_Name;
};

class GraphicsCommandContext : public CommandContext {
public:
    virtual void BeginRendering(Texture&                     render_target,
                                utils::optional_ref<Texture> depth_stencil       = {},
                                bool                         clear_render_target = false,
                                bool                         clear_depth_stencil = false) = 0;
    virtual void EndRendering()                                                           = 0;

    virtual void SetPipeline(const RenderPipeline& pipeline) = 0;

    virtual void SetViewPort(const ViewPort& view_port)   = 0;
    virtual void SetScissorRect(const Rect& scissor_rect) = 0;
    virtual void SetBlendColor(const math::Color& color)  = 0;

    virtual void SetIndexBuffer(const GPUBuffer& buffer, std::size_t offset = 0) = 0;
    virtual void SetVertexBuffers(
        std::uint8_t                                             start_binding,
        std::span<const std::reference_wrapper<const GPUBuffer>> buffers,
        std::span<const std::size_t>                             offsets) = 0;

    virtual void PushBindlessMetaInfo(const BindlessMetaInfo& info) = 0;

    virtual void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0)                                     = 0;
    virtual void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0, std::uint32_t base_vertex = 0, std::uint32_t first_instance = 0) = 0;

    virtual void CopyTextureRegion(
        const Texture&          src,
        math::vec3i             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer src_layer = {},
        TextureSubresourceLayer dst_layer = {}) = 0;

protected:
    GraphicsCommandContext(Device& device, std::string_view name) : CommandContext(device, CommandType::Graphics, name) {};
};

class ComputeCommandContext : public CommandContext {
public:
    virtual void SetPipeline(const ComputePipeline& pipeline) = 0;

    virtual void PushBindlessMetaInfo(const BindlessMetaInfo& info) = 0;

protected:
    ComputeCommandContext(Device& device, std::string_view name) : CommandContext(device, CommandType::Compute, name) {};
};

class CopyCommandContext : public CommandContext {
public:
    virtual void CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dst_offset, std::size_t size) = 0;
    virtual void CopyBufferToTexture(
        const GPUBuffer&        src,
        std::size_t             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer dst_layer = {}) = 0;

    virtual void CopyTextureToBuffer(
        const Texture&          src,
        math::vec3i             src_offset,
        math::vec3u             extent,
        GPUBuffer&              dst,
        std::size_t             dst_offset,
        TextureSubresourceLayer src_layer = {}) = 0;

    virtual void CopyTextureRegion(
        const Texture&          src,
        math::vec3i             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer src_layer = {},
        TextureSubresourceLayer dst_layer = {}) = 0;

protected:
    CopyCommandContext(Device& device, std::string_view name) : CommandContext(device, CommandType::Copy, name) {};
};

class CommandQueue {
public:
    CommandQueue(Device& device, CommandType type, std::string_view name) : m_Device(device), m_Type(type), m_Name(name) {}
    virtual ~CommandQueue() = default;

    inline auto& GetDevice() const noexcept { return m_Device; }
    inline auto  GetType() const noexcept { return m_Type; }
    inline auto& GetName() const noexcept { return m_Name; }

    virtual void Submit(
        std::span<const std::reference_wrapper<const CommandContext>> contexts,
        std::span<const FenceWaitInfo>                                wait_fences   = {},
        std::span<const FenceSignalInfo>                              signal_fences = {}) = 0;

    virtual void WaitIdle() = 0;

protected:
    Device&                m_Device;
    const CommandType      m_Type;
    const std::pmr::string m_Name;
};

inline auto split_semantic(std::string_view semantic) -> std::pair<std::string_view, std::uint32_t> {
    using namespace std::string_view_literals;

    constexpr std::array semantic_names = {
        "POSITION"sv,
        "NORMAL"sv,
        "TANGENT"sv,
        "BINORMAL"sv,
        "COLOR"sv,
        "TEXCOORD"sv,
        "BLENDINDICES"sv,
        "BLENDWEIGHT"sv,
        "PSIZE"sv,
    };

    for (const auto& name : semantic_names) {
        if (semantic.starts_with(name)) {
            if (semantic.size() == name.size()) {
                return {name, 0};
            } else {
                return {name, std::stoul(std::string{semantic.substr(name.size())})};
            }
        }
    }

    throw std::invalid_argument(std::format("Unkown semantic: {}", semantic));
}

inline constexpr auto get_format_bit_size(Format format) noexcept -> std::size_t {
    switch (format) {
        case Format::R32G32B32A32_TYPELESS:
        case Format::R32G32B32A32_FLOAT:
        case Format::R32G32B32A32_UINT:
        case Format::R32G32B32A32_SINT:
            return 128;
        case Format::R32G32B32_TYPELESS:
        case Format::R32G32B32_FLOAT:
        case Format::R32G32B32_UINT:
        case Format::R32G32B32_SINT:
            return 96;
        case Format::R16G16B16A16_TYPELESS:
        case Format::R16G16B16A16_FLOAT:
        case Format::R16G16B16A16_UNORM:
        case Format::R16G16B16A16_UINT:
        case Format::R16G16B16A16_SNORM:
        case Format::R16G16B16A16_SINT:
        case Format::R32G32_TYPELESS:
        case Format::R32G32_FLOAT:
        case Format::R32G32_UINT:
        case Format::R32G32_SINT:
        case Format::R32G8X24_TYPELESS:
        case Format::D32_FLOAT_S8X24_UINT:
        case Format::R32_FLOAT_X8X24_TYPELESS:
        case Format::X32_TYPELESS_G8X24_UINT:
            return 64;
        case Format::R10G10B10A2_TYPELESS:
        case Format::R10G10B10A2_UNORM:
        case Format::R10G10B10A2_UINT:
        case Format::R11G11B10_FLOAT:
        case Format::R8G8B8A8_TYPELESS:
        case Format::R8G8B8A8_UNORM:
        case Format::R8G8B8A8_UNORM_SRGB:
        case Format::R8G8B8A8_UINT:
        case Format::R8G8B8A8_SNORM:
        case Format::R8G8B8A8_SINT:
        case Format::R16G16_TYPELESS:
        case Format::R16G16_FLOAT:
        case Format::R16G16_UNORM:
        case Format::R16G16_UINT:
        case Format::R16G16_SNORM:
        case Format::R16G16_SINT:
        case Format::R32_TYPELESS:
        case Format::D32_FLOAT:
        case Format::R32_FLOAT:
        case Format::R32_UINT:
        case Format::R32_SINT:
        case Format::R24G8_TYPELESS:
        case Format::D24_UNORM_S8_UINT:
        case Format::R24_UNORM_X8_TYPELESS:
        case Format::X24_TYPELESS_G8_UINT:
            return 32;
        case Format::R8G8_TYPELESS:
        case Format::R8G8_UNORM:
        case Format::R8G8_UINT:
        case Format::R8G8_SNORM:
        case Format::R8G8_SINT:
        case Format::R16_TYPELESS:
        case Format::R16_FLOAT:
        case Format::D16_UNORM:
        case Format::R16_UNORM:
        case Format::R16_UINT:
        case Format::R16_SNORM:
        case Format::R16_SINT:
            return 16;
        case Format::R8_TYPELESS:
        case Format::R8_UNORM:
        case Format::R8_UINT:
        case Format::R8_SNORM:
        case Format::R8_SINT:
        case Format::A8_UNORM:
            return 8;
        case Format::R1_UNORM:
            return 1;
        default:
            return 0;
    }
}

inline constexpr auto get_format_byte_size(Format format) noexcept -> std::size_t {
    return get_format_bit_size(format) >> 3;
}

inline auto format_as(ResourceType type) noexcept {
    return magic_enum::enum_name(type);
}

inline auto format_as(GPUBufferUsageFlags usages) noexcept {
    return magic_enum::enum_flags_name(usages);
}

inline auto format_as(TextureUsageFlags usages) noexcept {
    return magic_enum::enum_flags_name(usages);
}

inline auto format_as(ShaderType type) noexcept {
    return magic_enum::enum_name(type);
}

inline auto format_as(Format format) noexcept {
    return magic_enum::enum_name(format);
}

inline auto formate_as(PrimitiveTopology topology) noexcept {
    return magic_enum::enum_name(topology);
}

inline auto format_as(FillMode mode) noexcept {
    return magic_enum::enum_name(mode);
}

inline auto format_as(CullMode mode) noexcept {
    return magic_enum::enum_name(mode);
}

inline auto format_as(FrontFace mode) noexcept {
    return magic_enum::enum_name(mode);
}

inline auto format_as(ColorMask mask) noexcept {
    return magic_enum::enum_flags_name(mask);
}

inline auto format_as(StencilOp op) noexcept {
    return magic_enum::enum_name(op);
}

inline auto format_as(BlendFactor factor) noexcept {
    return magic_enum::enum_name(factor);
}

inline auto format_as(BlendOp op) noexcept {
    return magic_enum::enum_name(op);
}

inline auto format_as(LogicOp op) noexcept {
    return magic_enum::enum_name(op);
}

inline auto format_as(CompareOp op) noexcept {
    return magic_enum::enum_name(op);
}

inline auto format_as(AddressMode mode) noexcept {
    return magic_enum::enum_name(mode);
}

inline auto format_as(FilterMode mode) noexcept {
    return magic_enum::enum_name(mode);
}

inline auto format_as(PipelineStage stage) noexcept {
    return magic_enum::enum_flags_name(stage);
}

inline auto format_as(BarrierAccess access) noexcept {
    return magic_enum::enum_flags_name(access);
}

inline auto format_as(TextureLayout layout) noexcept {
    return magic_enum::enum_flags_name(layout);
}

class ShaderCompiler {
public:
    ShaderCompiler(std::string_view name);
    ~ShaderCompiler();

    auto CompileToDXIL(const ShaderDesc& desc) const -> core::Buffer;
    auto CompileToSPIRV(const ShaderDesc& desc) const -> core::Buffer;

    auto ExtractVertexLayout(const ShaderDesc& desc) const -> VertexLayout;

private:
    auto CompileWithArgs(std::string_view source_code, const std::pmr::vector<std::pmr::wstring>& args) const -> ::IDxcResult*;
    auto GetShaderBuffer(::IDxcResult* result) const -> core::Buffer;

    std::shared_ptr<spdlog::logger> m_Logger;
    ::IDxcUtils*                    m_DxcUtils       = nullptr;
    ::IDxcCompiler3*                m_ShaderCompiler = nullptr;
};

class Device : public core::RuntimeModule {
public:
    enum struct Type : std::uint8_t {
        DX12,
        Vulkan,
        Mock
    } const device_type;

    virtual ~Device();

    void Tick() override;

    virtual void WaitIdle() = 0;

    virtual auto CreateFence(std::uint64_t initial_value = 0, std::string_view name = "") -> std::shared_ptr<Fence> = 0;

    virtual auto GetCommandQueue(CommandType type) const -> CommandQueue&                                              = 0;
    virtual auto CreateCommandContext(CommandType type, std::string_view name = "") -> std::shared_ptr<CommandContext> = 0;
    inline auto  CreateGraphicsContext(std::string_view name = "") -> std::shared_ptr<GraphicsCommandContext>;
    inline auto  CreateComputeContext(std::string_view name = "") -> std::shared_ptr<ComputeCommandContext>;
    inline auto  CreateCopyContext(std::string_view name = "") -> std::shared_ptr<CopyCommandContext>;

    virtual auto CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain>                                               = 0;
    virtual auto CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GPUBuffer> = 0;
    virtual auto CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture>       = 0;
    virtual auto CreateSampler(SamplerDesc desc) -> std::shared_ptr<Sampler>                                                     = 0;

    virtual auto CreateShader(ShaderDesc desc) -> std::shared_ptr<Shader>                            = 0;
    virtual auto CreateRenderPipeline(RenderPipelineDesc desc) -> std::shared_ptr<RenderPipeline>    = 0;
    virtual auto CreateComputePipeline(ComputePipelineDesc desc) -> std::shared_ptr<ComputePipeline> = 0;

    virtual auto GetBindlessUtils() -> BindlessUtils& = 0;

    inline void  SetProfile(bool enable) noexcept { m_EnableProfile = enable; }
    inline auto& GetShaderCompiler() const noexcept { return m_ShaderCompiler; }
    inline auto  GetLogger() const noexcept -> std::shared_ptr<spdlog::logger> { return m_Logger; }

protected:
    Device(Type type, std::string_view name);

    ShaderCompiler        m_ShaderCompiler;
    std::function<void()> report_debug_error_after_destroy_fn;

    std::size_t m_FrameIndex    = 0;
    bool        m_EnableProfile = false;
};

inline auto Device::CreateGraphicsContext(std::string_view name) -> std::shared_ptr<GraphicsCommandContext> {
    return std::static_pointer_cast<GraphicsCommandContext>(CreateCommandContext(CommandType::Graphics, name));
};
inline auto Device::CreateComputeContext(std::string_view name) -> std::shared_ptr<ComputeCommandContext> {
    return std::static_pointer_cast<ComputeCommandContext>(CreateCommandContext(CommandType::Compute, name));
};
inline auto Device::CreateCopyContext(std::string_view name) -> std::shared_ptr<CopyCommandContext> {
    return std::static_pointer_cast<CopyCommandContext>(CreateCommandContext(CommandType::Copy, name));
};

}  // namespace hitagi::gfx
