#pragma once
#include <hitagi/math/matrix.hpp>

#include <magic_enum.hpp>

#include <cstdint>
#include <type_traits>

namespace hitagi::graphics {

enum struct TextureAddressMode {
    Wrap,
    Mirror,
    Clamp,
    Border,
    MirrorOnce
};

enum struct ComparisonFunc {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

enum struct Filter : std::uint32_t {
    Min_Mag_Mip_Point                          = 0,
    Min_Mag_Point_Mip_Linear                   = 0x1,
    Min_Point_Mag_Linear_Mip_Point             = 0x4,
    Min_Point_Mag_Mip_Linear                   = 0x5,
    Min_Linear_Mag_Mip_Point                   = 0x10,
    Min_Linear_Mag_Point_Mip_Linear            = 0x11,
    Min_Mag_Linear_Mip_Point                   = 0x14,
    Min_Mag_Mip_Linear                         = 0x15,
    Anisotropic                                = 0x55,
    Comparison_Min_Mag_Mip_Point               = 0x80,
    Comparison_Min_Mag_Point_Mip_Linear        = 0x81,
    Comparison_Min_Point_Mag_Linear_Mip_Point  = 0x84,
    Comparison_Min_Point_Mag_Mip_Linear        = 0x85,
    Comparison_Min_Linear_Mag_Mip_Point        = 0x90,
    Comparison_Min_Linear_Mag_Point_Mip_Linear = 0x91,
    Comparison_Min_Mag_Linear_Mip_Point        = 0x94,
    Comparison_Min_Mag_Mip_Linear              = 0x95,
    Comparison_Anisotropic                     = 0xd5,
    Minimum_Min_Mag_Mip_Point                  = 0x100,
    Minimum_Min_Mag_Point_Mip_Linear           = 0x101,
    Minimum_Min_Point_Mag_Linear_Mip_Point     = 0x104,
    Minimum_Min_Point_Mag_Mip_Linear           = 0x105,
    Minimum_Min_Linear_Mag_Mip_Point           = 0x110,
    Minimum_Min_Linear_Mag_Point_Mip_Linear    = 0x111,
    Minimum_Min_Mag_Linear_Mip_Point           = 0x114,
    Minimum_Min_Mag_Mip_Linear                 = 0x115,
    Minimum_Anisotropic                        = 0x155,
    Maximum_Min_Mag_Mip_Point                  = 0x180,
    Maximum_Min_Mag_Point_Mip_Linear           = 0x181,
    Maximum_Min_Point_Mag_Linear_Mip_Point     = 0x184,
    Maximum_Min_Point_Mag_Mip_Linear           = 0x185,
    Maximum_Min_Linear_Mag_Mip_Point           = 0x190,
    Maximum_Min_Linear_Mag_Point_Mip_Linear    = 0x191,
    Maximum_Min_Mag_Linear_Mip_Point           = 0x194,
    Maximum_Min_Mag_Mip_Linear                 = 0x195,
    Maximum_Anisotropic                        = 0x1d5
};

// Pipeline
enum struct Blend {
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

enum struct BlendOp {
    Add,
    Subtract,
    RevSubtract,
    Min,
    Max
};

enum struct LogicOp {
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

enum struct FillMode {
    Solid,
    Wireframe,
};

enum struct CullMode {
    None,
    Front,
    Back
};

struct RasterizerDescription {
    FillMode fill_mode               = FillMode::Solid;
    CullMode cull_mode               = CullMode::None;
    bool     front_counter_clockwise = true;
    int      depth_bias              = 0;
    float    depth_bias_clamp        = 0.0f;
    float    slope_scaled_depth_bias = 0.0f;
    bool     depth_clip_enable       = true;
    bool     multisample_enable      = false;
    bool     antialiased_line_enable = false;
    unsigned forced_sample_count     = 0;
    bool     conservative_raster     = false;
};

}  // namespace hitagi::graphics