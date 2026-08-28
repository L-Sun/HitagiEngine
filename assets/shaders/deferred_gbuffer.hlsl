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
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 pos_in_view : POSITION;
    float3 normal_in_view : NORMAL;
    float2 uv : TEXCOORD0;
};

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
    const float3 n = normalize(normal_in_view);
    const float3 q1 = ddx(pos_in_view);
    const float3 q2 = ddy(pos_in_view);
    const float2 st1 = ddx(uv);
    const float2 st2 = ddy(uv);

    const float3 q2_perp = cross(q2, n);
    const float3 q1_perp = cross(n, q1);
    const float3 t = q2_perp * st1.x + q1_perp * st2.x;
    const float3 b = q2_perp * st1.y + q1_perp * st2.y;
    const float inv_len = rsqrt(max(dot(t, t), dot(b, b)) + 1e-6f);
    const float3x3 tbn = float3x3(t * inv_len, b * inv_len, n);

    return normalize(mul(normal_sample * 2.0f - 1.0f, tbn));
}

float4 PSAlbedoMain(PSInput input) : SV_TARGET {
    const BindlessInfo resource      = hitagi::load_bindless<BindlessInfo>();
    const MaterialData material_data = resource.material_data.load<MaterialData>();
    const SamplerState sampler       = resource.base_sampler.load();

    float4 base_color = material_data.base_color;
    if (hitagi::valid(material_data.base_color_texture)) {
        const float4 texel = material_data.base_color_texture.sample<float4>(sampler, input.uv);
        base_color *= float4(SRGBToLinear(texel.rgb), texel.a);
    }
    if (material_data.alpha_cutout != 0 && base_color.a < material_data.alpha_cutoff) {
        discard;
    }

    const float linear_depth = max(-input.pos_in_view.z, 0.0f);
    return float4(saturate(base_color.rgb), linear_depth);
}

float4 PSNormalMain(PSInput input) : SV_TARGET {
    const BindlessInfo resource      = hitagi::load_bindless<BindlessInfo>();
    const MaterialData material_data = resource.material_data.load<MaterialData>();
    const SamplerState sampler       = resource.base_sampler.load();

    float3 normal = normalize(input.normal_in_view);
    if (hitagi::valid(material_data.normal_texture)) {
        normal = BuildNormalFromMap(
            normal,
            input.pos_in_view,
            input.uv,
            material_data.normal_texture.sample<float3>(sampler, input.uv));
    }

    const float texture_occlusion = hitagi::valid(material_data.occlusion_texture)
                                        ? material_data.occlusion_texture.sample<float>(sampler, input.uv)
                                        : 1.0f;
    const float occlusion = saturate(material_data.occlusion * texture_occlusion);

    return float4(normal * 0.5f + 0.5f, occlusion);
}

float4 PSMaterialMain(PSInput input) : SV_TARGET {
    const BindlessInfo resource      = hitagi::load_bindless<BindlessInfo>();
    const MaterialData material_data = resource.material_data.load<MaterialData>();
    const SamplerState sampler       = resource.base_sampler.load();

    float metallic = saturate(material_data.metallic);
    float roughness = clamp(material_data.roughness, 0.04f, 1.0f);

    if (hitagi::valid(material_data.metallic_roughness_texture)) {
        const float3 mr = material_data.metallic_roughness_texture.sample<float3>(sampler, input.uv);
        roughness = mr.g;
        metallic = mr.b;
    }

    return float4(saturate(metallic), clamp(roughness, 0.04f, 1.0f), 0.0f, 1.0f);
}

float4 PSEmissiveMain(PSInput input) : SV_TARGET {
    const BindlessInfo resource      = hitagi::load_bindless<BindlessInfo>();
    const MaterialData material_data = resource.material_data.load<MaterialData>();
    const SamplerState sampler       = resource.base_sampler.load();

    const float3 emissive = hitagi::valid(material_data.emissive_texture)
                                ? SRGBToLinear(material_data.emissive_texture.sample<float3>(sampler, input.uv))
                                : saturate(material_data.emissive_color.xyz);

    return float4(emissive, 1.0f);
}
