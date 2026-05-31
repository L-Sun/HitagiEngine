#include "bindless.hlsl"

struct SelectionMaskBindlessInfo {
    hitagi::SimpleBuffer frame_constant;
    hitagi::SimpleBuffer instance_constant;
};

struct SelectionOutlineBindlessInfo {
    hitagi::SimpleBuffer outline_constant;
    hitagi::Texture      scene_color;
    hitagi::Texture      scene_depth;
    hitagi::Texture      selection_id;
    hitagi::Texture      selection_visual;
    hitagi::Texture      selection_depth;
    hitagi::Sampler      sampler;
};

struct FrameConstant {
    float4 camera_pos;
    matrix view;
    matrix projection;
    matrix proj_view;
    matrix inv_view;
    matrix inv_projection;
    matrix inv_proj_view;
};

struct SelectionInstanceConstant {
    matrix model;
    uint   selection_id;
    uint   visual_id;
    uint2  padding;
};

struct SelectionOutlineConstant {
    float4 selected_color;
    float4 hovered_color;
    float4 occluded_color;
    float4 params; // width_px, highlight_strength, show_occluded, depth_threshold
    float4 viewport; // width, height, inv_width, inv_height
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct SelectionPSInput {
    float4 position : SV_POSITION;
    float  linear_depth : TEXCOORD0;
};

struct FullscreenPSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

SelectionPSInput VSSelectionMaskMain(VSInput input) {
    const SelectionMaskBindlessInfo resource = hitagi::load_bindless<SelectionMaskBindlessInfo>();
    const FrameConstant frame = resource.frame_constant.load<FrameConstant>();
    const SelectionInstanceConstant instance = resource.instance_constant.load<SelectionInstanceConstant>();

    const float4 world_pos = mul(instance.model, float4(input.position, 1.0f));
    const float3 view_pos = mul(frame.view, world_pos).xyz;

    SelectionPSInput output;
    output.position = mul(frame.proj_view, world_pos);
    output.linear_depth = max(-view_pos.z, 0.0f);
    return output;
}

uint PSSelectionIdMain(SelectionPSInput input) : SV_TARGET {
    const SelectionMaskBindlessInfo resource = hitagi::load_bindless<SelectionMaskBindlessInfo>();
    const SelectionInstanceConstant instance = resource.instance_constant.load<SelectionInstanceConstant>();
    return instance.selection_id;
}

uint PSSelectionVisualMain(SelectionPSInput input) : SV_TARGET {
    const SelectionMaskBindlessInfo resource = hitagi::load_bindless<SelectionMaskBindlessInfo>();
    const SelectionInstanceConstant instance = resource.instance_constant.load<SelectionInstanceConstant>();
    return instance.visual_id;
}

float PSSelectionDepthMain(SelectionPSInput input) : SV_TARGET {
    return input.linear_depth;
}

FullscreenPSInput VSFullscreenMain(uint vertex_id : SV_VertexID) {
    static const float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f),
    };

    const float2 position = positions[vertex_id];

    FullscreenPSInput output;
    output.position = float4(position, 0.0f, 1.0f);
    output.uv = float2(position.x * 0.5f + 0.5f, 0.5f - position.y * 0.5f);
    return output;
}

float4 VisualColor(uint visual_id, SelectionOutlineConstant constant) {
    if (visual_id == 2u) return constant.hovered_color;
    return constant.selected_color;
}

uint LoadSelectionId(SelectionOutlineBindlessInfo resource, int2 pixel, int2 size) {
    pixel = clamp(pixel, int2(0, 0), size - int2(1, 1));
    return resource.selection_id.load<uint>(uint2(pixel));
}

uint LoadSelectionVisual(SelectionOutlineBindlessInfo resource, int2 pixel, int2 size) {
    pixel = clamp(pixel, int2(0, 0), size - int2(1, 1));
    return resource.selection_visual.load<uint>(uint2(pixel));
}

float LoadSelectionDepth(SelectionOutlineBindlessInfo resource, int2 pixel, int2 size) {
    pixel = clamp(pixel, int2(0, 0), size - int2(1, 1));
    return resource.selection_depth.load<float>(uint2(pixel));
}

float4 PSSelectionOutlineMain(FullscreenPSInput input) : SV_TARGET {
    const SelectionOutlineBindlessInfo resource = hitagi::load_bindless<SelectionOutlineBindlessInfo>();
    const SelectionOutlineConstant constant = resource.outline_constant.load<SelectionOutlineConstant>();
    const SamplerState sampler = resource.sampler.load();

    const float4 scene = resource.scene_color.sample_level<float4>(sampler, input.uv, 0.0f);
    const int2 size = int2(max(constant.viewport.x, 1.0f), max(constant.viewport.y, 1.0f));
    const int2 pixel = clamp(int2(input.position.xy), int2(0, 0), size - int2(1, 1));

    const uint center_id = LoadSelectionId(resource, pixel, size);
    const float outline_width = max(constant.params.x, 1.0f);
    const int radius = max(1, (int)ceil(outline_width));

    uint neighbor_id = 0;
    uint neighbor_visual = 1;
    float neighbor_depth = 0.0f;

    [unroll]
    for (int y = -3; y <= 3; ++y) {
        [unroll]
        for (int x = -3; x <= 3; ++x) {
            if (x == 0 && y == 0) continue;
            if (abs(x) > radius || abs(y) > radius) continue;
            const int2 sample_pixel = pixel + int2(x, y);
            const uint sample_id = LoadSelectionId(resource, sample_pixel, size);
            if (sample_id != 0u && sample_id != center_id) {
                neighbor_id = sample_id;
                neighbor_visual = LoadSelectionVisual(resource, sample_pixel, size);
                neighbor_depth = LoadSelectionDepth(resource, sample_pixel, size);
            }
        }
    }

    if (center_id != 0u) {
        const uint visual = LoadSelectionVisual(resource, pixel, size);
        const float4 color = VisualColor(visual, constant);
        return float4(lerp(scene.rgb, color.rgb, saturate(constant.params.y)), scene.a);
    }

    if (neighbor_id == 0u) return scene;

    const float scene_linear_depth = resource.scene_depth.load<float4>(uint2(pixel)).a;
    const bool has_scene_depth = scene_linear_depth > 0.00001f;
    const bool occluded = has_scene_depth && neighbor_depth > scene_linear_depth + constant.params.w;
    const bool show_occluded = constant.params.z > 0.5f;

    float4 outline_color = VisualColor(neighbor_visual, constant);
    if (occluded) {
        if (!show_occluded) return scene;
        outline_color = constant.occluded_color;
    }

    return float4(lerp(scene.rgb, outline_color.rgb, saturate(outline_color.a)), scene.a);
}
