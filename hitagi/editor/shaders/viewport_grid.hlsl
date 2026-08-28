#include "bindless.hlsl"

struct BindlessInfo {
    hitagi::SimpleBuffer grid_constant;
};

struct GridConstant {
    matrix inv_proj_view;
    float4 camera_pos;
    float4 camera_forward;
    float4 viewport_size_base_step_fade;
    float4 clip_and_opacity;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

VSOutput VSMain(uint vertex_id : SV_VertexID) {
    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f),
    };

    VSOutput output;
    output.position = float4(positions[vertex_id], 0.0f, 1.0f);
    return output;
}

float grid_line_alpha(float2 world_xy, float step) {
    float2 coord = world_xy / step;
    float2 width = max(fwidth(coord), float2(1e-4f, 1e-4f));
    float2 cell = abs(frac(coord - 0.5f) - 0.5f) / width;
    return 1.0f - saturate(min(cell.x, cell.y));
}

float axis_line_alpha(float value) {
    float width = max(fwidth(value), 1e-4f);
    return 1.0f - saturate(abs(value) / width);
}

float4 PSMain(VSOutput input) : SV_TARGET {
    BindlessInfo bindless_info = hitagi::load_bindless<BindlessInfo>();
    GridConstant grid = bindless_info.grid_constant.load<GridConstant>();

    float2 viewport_size = grid.viewport_size_base_step_fade.xy;
    float base_step = grid.viewport_size_base_step_fade.z;
    float lod_fade = grid.viewport_size_base_step_fade.w;
    float far_clip = grid.clip_and_opacity.x;
    float opacity = grid.clip_and_opacity.y;

    float2 uv = input.position.xy / max(viewport_size, float2(1.0f, 1.0f));
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 far_pos = mul(grid.inv_proj_view, float4(ndc, 1.0f, 1.0f));
    far_pos.xyz /= max(abs(far_pos.w), 1e-6f);

    float3 origin = grid.camera_pos.xyz;
    float3 ray = normalize(far_pos.xyz - origin);
    if (abs(ray.z) < 1e-5f) {
        discard;
    }

    float t = -origin.z / ray.z;
    if (t <= 0.0f) {
        discard;
    }

    float3 world = origin + ray * t;
    float distance_to_camera = length(world - origin);

    float floor_angle_fade = 1.0f - pow(1.0f - abs(grid.camera_forward.z), 3.0f);
    float distance_fade = 1.0f - smoothstep(far_clip * 0.45f, far_clip * 0.95f, distance_to_camera);

    float minor = grid_line_alpha(world.xy, base_step * 0.1f) * (1.0f - lod_fade);
    float major = grid_line_alpha(world.xy, base_step);
    float coarse = grid_line_alpha(world.xy, base_step * 10.0f) * lod_fade;

    float line_alpha = max(max(minor * 0.35f, major * 0.62f), coarse * 0.85f);
    float3 color = lerp(float3(0.25f, 0.32f, 0.40f), float3(0.46f, 0.55f, 0.66f), saturate(coarse + major * 0.5f));

    float x_axis = axis_line_alpha(world.y);
    float y_axis = axis_line_alpha(world.x);
    color = lerp(color, float3(0.26f, 0.72f, 0.36f), x_axis);
    color = lerp(color, float3(0.88f, 0.26f, 0.26f), y_axis);
    line_alpha = max(line_alpha, max(x_axis, y_axis) * 0.82f);

    float alpha = line_alpha * floor_angle_fade * distance_fade * opacity;
    if (alpha <= 0.001f) {
        discard;
    }
    return float4(color, alpha);
}
