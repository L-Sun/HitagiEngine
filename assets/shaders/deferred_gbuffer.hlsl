#include "bindless.hlsl"

struct BindlessInfo {
    hitagi::SimpleBuffer frame_constant;
    hitagi::SimpleBuffer instance_constant;
    hitagi::SimpleBuffer material_constant;
    hitagi::Texture      diffuse_texture;
    hitagi::Texture      specular_texture;
    hitagi::Texture      ambient_texture;
    hitagi::Texture      emissive_texture;
    hitagi::Texture      metallic_roughness_texture;
    hitagi::Texture      normal_texture;
    hitagi::Texture      occlusion_texture;
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

struct MaterialConstant {
    float  shininess;
    float  roughness;
    float  metallic;
    float  occlusion;
    float4 diffuse;
    float4 specular;
    float4 ambient;
    float4 emissive;
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

float ShininessToRoughness(float shininess) {
    return clamp(sqrt(2.0f / max(shininess + 2.0f, 2.0f)), 0.04f, 1.0f);
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
    const BindlessInfo     resource          = hitagi::load_bindless<BindlessInfo>();
    const MaterialConstant material_constant = resource.material_constant.load<MaterialConstant>();
    const SamplerState     sampler           = resource.base_sampler.load();

    const float3 diffuse = hitagi::valid(resource.diffuse_texture)
                               ? SRGBToLinear(resource.diffuse_texture.sample<float3>(sampler, input.uv))
                               : saturate(material_constant.diffuse.xyz);

    const float linear_depth = max(-input.pos_in_view.z, 0.0f);
    return float4(saturate(diffuse), linear_depth);
}

float4 PSNormalMain(PSInput input) : SV_TARGET {
    const BindlessInfo     resource          = hitagi::load_bindless<BindlessInfo>();
    const MaterialConstant material_constant = resource.material_constant.load<MaterialConstant>();
    const SamplerState     sampler           = resource.base_sampler.load();

    float3 normal = normalize(input.normal_in_view);
    if (hitagi::valid(resource.normal_texture)) {
        normal = BuildNormalFromMap(
            normal,
            input.pos_in_view,
            input.uv,
            resource.normal_texture.sample<float3>(sampler, input.uv));
    }

    const float texture_occlusion = hitagi::valid(resource.occlusion_texture)
                                        ? resource.occlusion_texture.sample<float>(sampler, input.uv)
                                        : 1.0f;
    const float occlusion = saturate(material_constant.occlusion * texture_occlusion);

    return float4(normal * 0.5f + 0.5f, occlusion);
}

float4 PSMaterialMain(PSInput input) : SV_TARGET {
    const BindlessInfo     resource          = hitagi::load_bindless<BindlessInfo>();
    const MaterialConstant material_constant = resource.material_constant.load<MaterialConstant>();
    const SamplerState     sampler           = resource.base_sampler.load();

    float metallic = saturate(material_constant.metallic);
    float roughness = saturate(material_constant.roughness);
    if (roughness <= 0.0f) {
        roughness = ShininessToRoughness(material_constant.shininess);
    }

    if (hitagi::valid(resource.metallic_roughness_texture)) {
        const float3 mr = resource.metallic_roughness_texture.sample<float3>(sampler, input.uv);
        roughness = mr.g;
        metallic = mr.b;
    }

    return float4(saturate(metallic), clamp(roughness, 0.04f, 1.0f), 0.0f, 1.0f);
}

float4 PSEmissiveMain(PSInput input) : SV_TARGET {
    const BindlessInfo     resource          = hitagi::load_bindless<BindlessInfo>();
    const MaterialConstant material_constant = resource.material_constant.load<MaterialConstant>();
    const SamplerState     sampler           = resource.base_sampler.load();

    const float3 emissive = hitagi::valid(resource.emissive_texture)
                                ? SRGBToLinear(resource.emissive_texture.sample<float3>(sampler, input.uv))
                                : saturate(material_constant.emissive.xyz);

    return float4(emissive, 1.0f);
}
