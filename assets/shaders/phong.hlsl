#include "bindless.hlsl"

struct BindlessInfo {
    hitagi::SimpleBuffer frame_constant;
    hitagi::SimpleBuffer instance_constant;
    hitagi::SimpleBuffer material_constant;
    hitagi::Texture      diffuse_texture;
    hitagi::Texture      specular_texture;
    hitagi::Texture      ambient_texture;
    hitagi::Texture      emissive_texture;
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

struct MaterialConstant {
    float  shininess;
    float3 diffuse;
    float3 specular;
    float3 ambient;
    float3 emissive;
};

struct VSInput {
    float3 position : POSITION;
    // float4 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSInput {
    float4 position : SV_POSITION;
    // float4 color : COLOR;
    float3 pos_in_view : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(VSInput input) {
    BindlessInfo     resource          = hitagi::load_bindless<BindlessInfo>();
    FrameConstant    frame_constant    = resource.frame_constant.load<FrameConstant>();
    InstanceConstant instance_constant = resource.instance_constant.load<InstanceConstant>();

    PSInput output;

    matrix MVP = mul(frame_constant.proj_view, instance_constant.model);

    output.position    = mul(MVP, float4(input.position, 1.0f));
    output.normal      = mul(frame_constant.view, mul(instance_constant.model, float4(input.normal, 0.0f))).xyz;
    output.pos_in_view = mul(frame_constant.view, mul(instance_constant.model, float4(input.position, 1.0f))).xyz;
    // output.color       = input.color;
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input)
    : SV_TARGET {
    const BindlessInfo     resource          = hitagi::load_bindless<BindlessInfo>();
    const FrameConstant    frame_constant    = resource.frame_constant.load<FrameConstant>();
    const MaterialConstant material_constant = resource.material_constant.load<MaterialConstant>();
    const SamplerState     sampler           = resource.base_sampler.load();

    const float3 vN   = normalize(input.normal);
    const float3 vL   = normalize(frame_constant.light_pos_in_view.xyz - input.pos_in_view);
    const float3 vV   = normalize(-input.pos_in_view);
    const float3 vH   = normalize(vL + vV);
    const float  r    = length(frame_constant.light_pos_in_view.xyz - input.pos_in_view);
    const float  invd = 1.0f / (r * r + 1.0f);

    // color
    const float3 _diffuse  = hitagi::valid(resource.diffuse_texture) ? resource.diffuse_texture.sample<float3>(sampler, input.uv) : material_constant.diffuse.xyz;
    const float3 _specular = hitagi::valid(resource.specular_texture) ? resource.specular_texture.sample<float3>(sampler, input.uv) : material_constant.specular.xyz;
    const float3 _ambient  = hitagi::valid(resource.ambient_texture) ? resource.ambient_texture.sample<float3>(sampler, input.uv) : material_constant.ambient.xyz;
    const float3 _emissive = hitagi::valid(resource.emissive_texture) ? resource.emissive_texture.sample<float3>(sampler, input.uv) : material_constant.emissive.xyz;

    const float3 vLightInts = _ambient + (frame_constant.light_color * frame_constant.light_intensity) * invd * (_diffuse * max(dot(vN, vL), 0.0f) + _specular * pow(max(dot(vH, vN), 0.0f), material_constant.shininess));

    return float4(vLightInts, 1.0f);
}
