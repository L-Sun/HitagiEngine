import bindless;

// CPU mirrors: render::toon::{ToonBindlessInfo,ToonFrameConstant,ToonInstanceConstant}.
struct BindlessInfo {
    hitagi::SimpleBuffer frame_constant;
    hitagi::SimpleBuffer instance_constant;
    uint instance_index;
    uint instance_stride;
    hitagi::SimpleBuffer material_data;
    hitagi::Sampler sampler;
};
struct FrameConstant {
    row_major float4x4 view;
    row_major float4x4 projection;
    row_major float4x4 proj_view;
    float4 light_direction;
    float4 light_color;
    float4 viewport;
};
struct InstanceConstant {
    row_major float4x4 model;
    row_major float4x4 normal_matrix;
};
struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};
struct PSInput {
    float4 position : SV_POSITION;
    float3 pos_in_view : POSITION;
    float3 normal_in_view : NORMAL;
    float2 uv : TEXCOORD0;
};
struct SurfaceInput {
    float2 uv;
    float3 normal;
    bool is_vertex;
};
struct MaterialContext {
    hitagi::SimpleBuffer material_data;
    SamplerState sampler;
};
// Colors are authored/display sRGB. This artistic ramp intentionally preserves
// texture colors instead of applying a physically based BRDF or inverse-square falloff.
struct Surface {
    float4 base_color;
    float4 shadow_color;
    float4 outline_color;
    float shadow_threshold;
    float light_threshold;
    float band_softness;
    float specular_threshold;
    float specular_strength;
    float rim_strength;
    float outline_width;
    float opacity;
    uint alpha_cutout;
    float alpha_cutoff;
};
interface IMaterial {
    static void evaluate(SurfaceInput input, MaterialContext context, inout Surface surface);
}
Surface DefaultSurface() {
    Surface s;
    s.base_color = float4(1,1,1,1);
    s.shadow_color = float4(0.63,0.65,0.78,1);
    s.outline_color = float4(0.055,0.045,0.085,1);
    s.shadow_threshold = 0.1;
    s.light_threshold = 0.65;
    s.band_softness = 0.035;
    s.specular_threshold = 0.96;
    s.specular_strength = 0.08;
    s.rim_strength = 0.06;
    s.outline_width = 1.0;
    s.opacity = 1.0;
    s.alpha_cutout = 0;
    s.alpha_cutoff = 0.5;
    return s;
}
Surface Evaluate<M : IMaterial>(float2 uv, float3 normal, bool vertex) {
    const BindlessInfo r = hitagi::load_bindless<BindlessInfo>();
    SurfaceInput input;
    input.uv = uv;
    input.normal = normal;
    input.is_vertex = vertex;
    MaterialContext context;
    context.material_data = r.material_data;
    context.sampler = r.sampler.load();
    Surface surface = DefaultSurface();
    M.evaluate(input, context, surface);
    return surface;
}
PSInput Vertex(VSInput input) {
    const BindlessInfo r = hitagi::load_bindless<BindlessInfo>();
    const FrameConstant frame = r.frame_constant.load<FrameConstant>();
    const InstanceConstant instance = r.instance_constant.load<InstanceConstant>(r.instance_index * r.instance_stride);
    const float4 world = mul(instance.model, float4(input.position,1));
    PSInput output;
    output.position = mul(frame.proj_view, world);
    output.pos_in_view = mul(frame.view, world).xyz;
    output.normal_in_view = normalize(mul(frame.view, mul(instance.normal_matrix, float4(input.normal,0))).xyz);
    output.uv = input.uv;
    return output;
}
PSInput VSMain<M : IMaterial>(VSInput input) { return Vertex(input); }

PSInput VSOutline<M : IMaterial>(VSInput input) {
    PSInput output = Vertex(input);
    const BindlessInfo r = hitagi::load_bindless<BindlessInfo>();
    const FrameConstant frame = r.frame_constant.load<FrameConstant>();
    const Surface surface = Evaluate<M>(input.uv, output.normal_in_view, true);
    // Project the normal as a direction. Normalize in pixel coordinates before
    // expanding, so aspect ratio and camera distance do not change line width.
    const float4 clip_normal = mul(frame.projection, float4(output.normal_in_view,0));
    const float2 projected = clip_normal.xy * output.position.w - output.position.xy * clip_normal.w;
    const float2 pixel_normal = projected * frame.viewport.xy;
    const float len = length(pixel_normal);
    const float2 direction = len > 1e-6 ? pixel_normal / len : float2(0,0);
    const float width = clamp(surface.outline_width,0.0,8.0);
    output.position.xy += direction * (2.0 * width * frame.viewport.zw) * output.position.w;
    return output;
}
float Band(float value, float threshold, float softness) {
    const float width = max(max(softness, fwidth(value)), 0.001);
    return smoothstep(threshold-width, threshold+width, value);
}
float4 PSMain<M : IMaterial>(PSInput input, bool front : SV_IsFrontFace) : SV_TARGET {
    const BindlessInfo r = hitagi::load_bindless<BindlessInfo>();
    const FrameConstant frame = r.frame_constant.load<FrameConstant>();
    const float3 n = normalize(input.normal_in_view) * (front ? 1.0 : -1.0);
    const Surface s = Evaluate<M>(input.uv,n,false);
    const float3 l = normalize(frame.light_direction.xyz);
    const float3 v = normalize(-input.pos_in_view + float3(0,0,1e-6));
    const float ndl = dot(n,l);
    const float shade = Band(ndl,s.shadow_threshold,s.band_softness);
    const float lit = Band(ndl,max(s.light_threshold,s.shadow_threshold+0.01),s.band_softness);
    const float3 ramp = lerp(s.shadow_color.rgb, lerp(s.shadow_color.rgb, float3(1,1,1),0.6),shade);
    const float3 diffuse = lerp(ramp,float3(1,1,1),lit);
    const float3 h = normalize(v+l+float3(0,0,1e-6));
    const float specular = Band(saturate(dot(n,h)),s.specular_threshold,0.006) * lit * max(s.specular_strength,0.0);
    const float rim = Band(1.0-saturate(dot(n,v)),0.78,0.04) * shade * max(s.rim_strength,0.0);
    const float alpha = saturate(s.base_color.a*s.opacity);
    if (s.alpha_cutout != 0 && alpha < s.alpha_cutoff) discard;
    const float3 color = s.base_color.rgb * diffuse * frame.light_color.rgb * frame.light_color.a + specular + rim;
    return float4(saturate(color),alpha);
}
float4 PSOutline<M : IMaterial>(PSInput input) : SV_TARGET {
    const Surface s = Evaluate<M>(input.uv,normalize(input.normal_in_view),false);
    const float alpha = saturate(s.base_color.a*s.opacity);
    if (s.outline_width <= 0 || (s.alpha_cutout != 0 && alpha < s.alpha_cutoff)) discard;
    return float4(s.outline_color.rgb,1.0);
}
