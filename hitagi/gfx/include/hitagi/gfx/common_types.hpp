#pragma once
#include <hitagi/math/vector.hpp>
#include <hitagi/core/buffer.hpp>

#include <cstdint>
#include <vector>

namespace hitagi::gfx {
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

inline constexpr std::size_t get_format_bit_size(Format format) noexcept {
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

struct Shader {
    enum struct Type : std::uint8_t {
        Vertex,
        Pixel,
        Geometry,
        Compute,
    };

    std::pmr::string name;
    Type             type;
    std::pmr::string entry;
    std::pmr::string source_code;
    core::Buffer     binary_data = {};
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
    // clockwise
    CW,
    // counter-clockwise
    CCW,
};

enum struct Blend : std::uint8_t {
    Zero,
    One,
    SrcColor,
    InvSrcColor,
    SrcAlpha,
    InvSrcAlpha,
    DestAlpha,
    InvDestAlpha,
    DestColor,
    InvDestColor,
    SrcAlphaSat,
    BlendFactor,
    InvBlendFactor,
    Src1Color,
    InvSrc_1_Color,
    Src1Alpha,
    InvSrc_1_Alpha,
};

enum struct BlendOp : std::uint8_t {
    Add,
    Subtract,
    RevSubtract,
    Min,
    Max
};

enum struct LogicOp : std::uint8_t {
    Clear,
    Set,
    Copy,
    CopyInverted,
    Noop,
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

enum struct CompareFunction : std::uint8_t {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

struct RasterizerDescription {
    FillMode fill_mode               = FillMode::Solid;
    CullMode cull_mode               = CullMode::Back;
    bool     front_counter_clockwise = false;
    int      depth_bias              = 0;
    float    depth_bias_clamp        = 0.0f;
    float    slope_scaled_depth_bias = 0.0f;
    bool     depth_clip_enable       = true;
    bool     multisample_enable      = false;
    bool     antialiased_line_enable = false;
    unsigned forced_sample_count     = 0;
    bool     conservative_raster     = false;
};

struct BlendDescription {
    bool    alpha_to_coverage_enable = false;
    bool    independent_blend_enable = false;
    bool    enable_blend             = false;
    bool    enable_logic_operation   = false;
    Blend   src_blend                = Blend::One;
    Blend   dest_blend               = Blend::Zero;
    BlendOp blend_op                 = BlendOp::Add;
    Blend   src_blend_alpha          = Blend::One;
    Blend   dest_blend_alpha         = Blend::Zero;
    BlendOp blend_op_alpha           = BlendOp::Add;
    LogicOp logic_op                 = LogicOp::Noop;
};

union ClearValue {
    math::vec4f color;
    struct {
        float         depth;
        std::uint32_t stencil;
    };
};

struct VertexAttribute {
    std::pmr::string semantic_name;
    std::uint8_t     semantic_index;
    std::uint8_t     slot;
    std::uint32_t    aligned_offset;
    Format           format;
    bool             per_instance            = false;
    std::uint32_t    instance_data_step_rate = 0;
};
using InputLayout = std::pmr::vector<VertexAttribute>;

struct ViewPort {
    float x, y;  // the top left start point
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

}  // namespace hitagi::gfx