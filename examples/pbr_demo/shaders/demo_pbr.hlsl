#include "bindless.hlsl"

struct BindlessInfo {
    hitagi::SimpleBuffer frame_constant;
    hitagi::SimpleBuffer instance_constant;
    hitagi::SimpleBuffer material_data;
    hitagi::Sampler      base_sampler;
};

struct FrameConstant {
    float4 camera_pos;
    matrix view;
    matrix projection;
    matrix proj_view;
    matrix inv_view;
    matrix inv_projection;
    matrix inv_proj_view;
    float4 light_position;
    float4 light_pos_in_view;
    float3 light_color;
    float  light_intensity;
};

struct InstanceConstant {
    matrix model;
};

struct MaterialData {
    float4 base_color;
    float  metallic;
    float  roughness;
    float  occlusion;
    float  _padding0;
    float4 emissive_color;
    float  opacity;
    uint   alpha_cutout;
    float  alpha_cutoff;
    float  _padding1;
    hitagi::Texture base_color_texture;
    hitagi::Texture metallic_roughness_texture;
    hitagi::Texture normal_texture;
    hitagi::Texture occlusion_texture;
    hitagi::Texture emissive_texture;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

struct PSInput {
    float4 position       : SV_POSITION;
    float3 pos_in_view    : POSITION;
    float3 normal_in_view : NORMAL;
    float2 uv             : TEXCOORD0;
};

static const float kPi = 3.14159265359f;

float3 SRGBToLinear(float3 color) {
    return pow(saturate(color), 2.2f);
}

PSInput VSMain(VSInput input) {
    BindlessInfo     resource          = hitagi::load_bindless<BindlessInfo>();
    FrameConstant    frame_constant    = resource.frame_constant.load<FrameConstant>();
    InstanceConstant instance_constant = resource.instance_constant.load<InstanceConstant>();

    const float4 world_pos = mul(instance_constant.model, float4(input.position, 1.0f));

    PSInput output;
    output.position       = mul(frame_constant.proj_view, world_pos);
    output.pos_in_view    = mul(frame_constant.view, world_pos).xyz;
    output.normal_in_view = normalize(mul(frame_constant.view, mul(instance_constant.model, float4(input.normal, 0.0f))).xyz);
    output.uv             = input.uv;
    return output;
}

float3 BuildNormalFromMap(float3 normal_in_view, float3 pos_in_view, float2 uv, float3 normal_sample) {
    const float3 n       = normalize(normal_in_view);
    const float3 q1      = ddx(pos_in_view);
    const float3 q2      = ddy(pos_in_view);
    const float2 st1     = ddx(uv);
    const float2 st2     = ddy(uv);
    const float3 q2_perp = cross(q2, n);
    const float3 q1_perp = cross(n, q1);
    const float3 t       = q2_perp * st1.x + q1_perp * st2.x;
    const float3 b       = q2_perp * st1.y + q1_perp * st2.y;
    const float  inv_len = rsqrt(max(dot(t, t), dot(b, b)) + 1e-6f);
    const float3x3 tbn   = float3x3(t * inv_len, b * inv_len, n);

    return normalize(mul(normal_sample * 2.0f - 1.0f, tbn));
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

float4 PSMain(PSInput input) : SV_TARGET {
    const BindlessInfo  resource       = hitagi::load_bindless<BindlessInfo>();
    const FrameConstant frame_constant = resource.frame_constant.load<FrameConstant>();
    const MaterialData  material_data  = resource.material_data.load<MaterialData>();
    const SamplerState  sampler        = resource.base_sampler.load();

    float4 base_color = material_data.base_color;
    if (hitagi::valid(material_data.base_color_texture)) {
        const float4 texel = material_data.base_color_texture.sample<float4>(sampler, input.uv);
        base_color *= float4(SRGBToLinear(texel.rgb), texel.a);
    }

    const float alpha = saturate(base_color.a * material_data.opacity);
    if (material_data.alpha_cutout != 0 && alpha < material_data.alpha_cutoff) {
        discard;
    }

    float metallic  = saturate(material_data.metallic);
    float roughness = clamp(material_data.roughness, 0.04f, 1.0f);
    if (hitagi::valid(material_data.metallic_roughness_texture)) {
        const float3 mr = material_data.metallic_roughness_texture.sample<float3>(sampler, input.uv);
        roughness = clamp(mr.g, 0.04f, 1.0f);
        metallic  = saturate(mr.b);
    }

    float3 normal = normalize(input.normal_in_view);
    if (hitagi::valid(material_data.normal_texture)) {
        normal = BuildNormalFromMap(
            normal,
            input.pos_in_view,
            input.uv,
            material_data.normal_texture.sample<float3>(sampler, input.uv));
    }

    const float occlusion = saturate(material_data.occlusion *
                                     (hitagi::valid(material_data.occlusion_texture) ? material_data.occlusion_texture.sample<float>(sampler, input.uv) : 1.0f));
    const float3 emissive = hitagi::valid(material_data.emissive_texture)
                                ? SRGBToLinear(material_data.emissive_texture.sample<float3>(sampler, input.uv))
                                : material_data.emissive_color.rgb;

    const float3 light_pos = frame_constant.light_pos_in_view.xyz;
    const float3 light_vec = light_pos - input.pos_in_view;
    const float  distance  = max(length(light_vec), 1e-3f);
    const float3 l         = light_vec / distance;
    const float3 v         = normalize(-input.pos_in_view);
    const float3 h         = normalize(v + l);
    const float3 n         = normal;

    const float n_dot_l = saturate(dot(n, l));
    const float n_dot_v = saturate(dot(n, v));
    const float n_dot_h = saturate(dot(n, h));
    const float h_dot_v = saturate(dot(h, v));

    const float3 albedo = saturate(base_color.rgb);
    const float3 f0     = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    const float3 f      = FresnelSchlick(h_dot_v, f0);
    const float  d      = DistributionGGX(n_dot_h, roughness);
    const float  g      = GeometrySchlickGGX(n_dot_v, roughness) * GeometrySchlickGGX(n_dot_l, roughness);
    const float3 spec   = (d * g * f) / max(4.0f * n_dot_v * n_dot_l, 1e-5f);
    const float3 kd     = (1.0f - f) * (1.0f - metallic);

    const float attenuation = 1.0f / (distance * distance + 1.0f);
    const float3 radiance   = frame_constant.light_color * frame_constant.light_intensity * attenuation;
    const float3 ambient    = 0.03f * albedo * occlusion;
    const float3 color      = ambient + emissive + (kd * albedo / kPi + spec) * radiance * n_dot_l;

    return float4(saturate(color), alpha);
}
