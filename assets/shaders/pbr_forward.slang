import bindless;

// Mirrors render::pbr::PbrBindlessInfo, PbrFrameConstant and PbrInstanceConstant.
struct BindlessInfo {
    hitagi::SimpleBuffer frame_constant;
    hitagi::SimpleBuffer instance_constant;
    uint                 instance_index;
    uint                 instance_stride;
    hitagi::SimpleBuffer material_data;
    hitagi::Sampler      sampler;
};

struct FrameConstant {
    float4             camera_pos;
    row_major float4x4 view;
    row_major float4x4 projection;
    row_major float4x4 proj_view;
    row_major float4x4 inv_view;
    row_major float4x4 inv_projection;
    row_major float4x4 inv_proj_view;
    float4             light_position;
    float4             light_pos_in_view;
    float3             light_color;
    float              light_intensity;
};

struct InstanceConstant {
    row_major float4x4 model;
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

// Geometry visible to materials. `normal` is the normalized view-space normal.
struct SurfaceInput {
    float2 uv;
    float3 normal;
};

struct MaterialContext {
    hitagi::SimpleBuffer material_data;
    SamplerState         sampler;
};

// Material outputs. `base_color` is sRGB; `normal` is in tangent space.
struct Surface {
    float4 base_color;
    float  metallic;
    float  roughness;
    float  occlusion;
    float  opacity;
    uint   alpha_cutout;
    float  alpha_cutoff;
    float3 normal;
    float3 emissive;
};

interface IMaterial {
    static void evaluate(SurfaceInput input, MaterialContext context, inout Surface surface);
}

Surface DefaultSurface() {
    Surface surface;
    surface.base_color   = float4(1.0f, 1.0f, 1.0f, 1.0f);
    surface.metallic     = 0.0f;
    surface.roughness    = 0.5f;
    surface.occlusion    = 1.0f;
    surface.opacity      = 1.0f;
    surface.alpha_cutout = 0;
    surface.alpha_cutoff = 0.5f;
    surface.normal       = float3(0.0f, 0.0f, 1.0f);
    surface.emissive     = float3(0.0f, 0.0f, 0.0f);
    return surface;
}

static const float kPi = 3.14159265359f;

float3 SRGBToLinear(float3 color) {
    return pow(saturate(color), 2.2f);
}

// Screen-space tangent frame, so meshes need no tangent stream.
float3 TangentToView(float3 normal, float3 pos_in_view, float2 uv, float3 tangent_normal) {
    const float3   q1      = ddx(pos_in_view);
    const float3   q2      = ddy(pos_in_view);
    const float2   st1     = ddx(uv);
    const float2   st2     = ddy(uv);
    const float3   q2_perp = cross(q2, normal);
    const float3   q1_perp = cross(normal, q1);
    const float3   t       = q2_perp * st1.x + q1_perp * st2.x;
    const float3   b       = q2_perp * st1.y + q1_perp * st2.y;
    const float    inv_len = rsqrt(max(dot(t, t), dot(b, b)) + 1e-6f);
    const float3x3 tbn     = float3x3(t * inv_len, b * inv_len, normal);
    return normalize(mul(tangent_normal, tbn));
}

float DistributionGGX(float n_dot_h, float roughness) {
    const float a  = roughness * roughness;
    const float a2 = a * a;
    const float d  = n_dot_h * n_dot_h * (a2 - 1.0f) + 1.0f;
    return a2 / max(kPi * d * d, 1e-5f);
}

float GeometrySchlickGGX(float n_dot_v, float roughness) {
    const float r = roughness + 1.0f;
    const float k = (r * r) / 8.0f;
    return n_dot_v / max(n_dot_v * (1.0f - k) + k, 1e-5f);
}

float3 FresnelSchlick(float cos_theta, float3 f0) {
    return f0 + (1.0f - f0) * pow(saturate(1.0f - cos_theta), 5.0f);
}

PSInput VSMain<M : IMaterial>(VSInput input) {
    const BindlessInfo     resource          = hitagi::load_bindless<BindlessInfo>();
    const FrameConstant    frame_constant    = resource.frame_constant.load<FrameConstant>();
    const InstanceConstant instance_constant = resource.instance_constant.load<InstanceConstant>(resource.instance_index * resource.instance_stride);

    const float4 world_pos = mul(instance_constant.model, float4(input.position, 1.0f));
    PSInput      output;
    output.position       = mul(frame_constant.proj_view, world_pos);
    output.pos_in_view    = mul(frame_constant.view, world_pos).xyz;
    output.normal_in_view = normalize(mul(frame_constant.view, mul(instance_constant.model, float4(input.normal, 0.0f))).xyz);
    output.uv             = input.uv;
    return output;
}

float4 PSMain<M : IMaterial>(PSInput input) : SV_TARGET {
    const BindlessInfo  resource       = hitagi::load_bindless<BindlessInfo>();
    const FrameConstant frame_constant = resource.frame_constant.load<FrameConstant>();

    SurfaceInput surface_input;
    surface_input.uv     = input.uv;
    surface_input.normal = normalize(input.normal_in_view);
    MaterialContext context;
    context.material_data = resource.material_data;
    context.sampler       = resource.sampler.load();
    Surface surface       = DefaultSurface();
    M.evaluate(surface_input, context, surface);

    // Derivatives must be taken before discard.
    const float3 n = TangentToView(surface_input.normal, input.pos_in_view, input.uv, surface.normal);

    const float3 base_color = SRGBToLinear(surface.base_color.rgb);
    const float  alpha      = saturate(surface.base_color.a * surface.opacity);
    if (surface.alpha_cutout != 0 && alpha < surface.alpha_cutoff) discard;

    const float3 light_vec   = frame_constant.light_pos_in_view.xyz - input.pos_in_view;
    const float  distance    = max(length(light_vec), 1e-3f);
    const float3 l           = light_vec / distance;
    const float3 v           = normalize(-input.pos_in_view);
    const float3 h           = normalize(v + l);
    const float  n_dot_l     = saturate(dot(n, l));
    const float  n_dot_v     = saturate(dot(n, v));
    const float  n_dot_h     = saturate(dot(n, h));
    const float  h_dot_v     = saturate(dot(h, v));
    const float3 albedo      = saturate(base_color);
    const float  metallic    = saturate(surface.metallic);
    const float  roughness   = clamp(surface.roughness, 0.04f, 1.0f);
    const float3 f0          = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    const float3 f           = FresnelSchlick(h_dot_v, f0);
    const float  d           = DistributionGGX(n_dot_h, roughness);
    const float  g           = GeometrySchlickGGX(n_dot_v, roughness) * GeometrySchlickGGX(n_dot_l, roughness);
    const float3 spec        = (d * g * f) / max(4.0f * n_dot_v * n_dot_l, 1e-5f);
    const float3 kd          = (1.0f - f) * (1.0f - metallic);
    const float  attenuation = 1.0f / (distance * distance + 1.0f);
    const float3 radiance    = frame_constant.light_color * frame_constant.light_intensity * attenuation;
    const float3 ambient     = 0.03f * albedo * saturate(surface.occlusion);
    const float3 color       = ambient + surface.emissive + (kd * albedo / kPi + spec) * radiance * n_dot_l;
    return float4(saturate(color), alpha);
}
