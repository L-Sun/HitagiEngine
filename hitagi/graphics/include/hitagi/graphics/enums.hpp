#pragma once
#include <hitagi/math/matrix.hpp>
#include <hitagi/resource/enums.hpp>

#include <magic_enum.hpp>

#include <cstdint>
#include <type_traits>

namespace hitagi::graphics {
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