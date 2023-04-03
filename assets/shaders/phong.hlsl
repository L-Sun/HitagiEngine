#include "bindless.hlsl"

struct BindlessInfo {
    SimplBuffer frame_constant;
    SimplBuffer object_constant;
    SimplBuffer material_constant;
    Texture     textures[4];
    Sampler     base_sampler;
};

struct FrameConstants {
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

struct ObjectConstants {
    matrix model;
};

struct MaterialConstants {
    float3 diffuse;
    int    diffuse_texture;
    float3 specular;
    int    specular_texture;
    float3 ambient;
    int    ambient_texture;
    float3 emissive;
    int    emissive_texture;
    float  shininess;
}

struct VSInput {
    float3 position : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 pos_in_view : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(VSInput input) {
    Bindless       resource        = hitagi::load_bindless<Bindless>();
    FrameConstant  frame_constant  = resource.frame_constant.load<FrameConstant>();
    ObjectConstant object_constant = resource.object_constant.load<ObjectConstant>();

    PSInput output;

    matrix MVP = mul(frame_constant.proj_view, object_constant.model);

    output.position    = mul(MVP, float4(input.position, 1.0f));
    output.normal      = mul(frame_constant.view, mul(object_constant.model, float4(input.normal, 0.0f))).xyz;
    output.pos_in_view = mul(frame_constant.view, mul(object_constant.model, float4(input.position, 1.0f))).xyz;
    output.color       = input.color;
    output.uv          = input.uv;
    return output;
}

float4 PSMain(PSInput input)
    : SV_TARGET {
    Bindless         resource          = hitagi::load_bindless<Bindless>();
    FrameConstant    frame_constant    = resource.frame_constant.load<FrameConstant>();
    MaterialConstant material_constant = resource.material_constant.load<MaterialConstant>();

    const float3 vN   = normalize(input.normal);
    const float3 vL   = normalize(frame_constant.light_pos_in_view.xyz - input.pos_in_view);
    const float3 vV   = normalize(-input.pos_in_view);
    const float3 vH   = normalize(vL + vV);
    const float  r    = length(frame_constant.light_pos_in_view.xyz - input.pos_in_view);
    const float  invd = 1.0f / (r * r + 1.0f);
    // color
    const float3 _diffuse  = material_constant.diffuse_texture == -1 ? material_constant.diffuse : resource.textures[material_constant.diffuse_texture].sample(resource.base_sampler, input.uv).xyz;
    const float3 _specular = material_constant.specular_texture == -1 ? material_constant.specular : resource.textures[material_constant.specular_texture].sample(resource.base_sampler, input.uv).xyz;
    const float3 _ambient  = material_constant.ambient_texture == -1 ? material_constant.ambient : resource.textures[material_constant.ambient_texture].sample(resource.base_sampler, input.uv).xyz;
    const float3 _emissive = material_constant.emissive_texture == -1 ? material_constant.emissive : resource.textures[material_constant.emissive_texture].sample(resource.base_sampler, input.uv).xyz;

    float3 vLightInts = _ambient + (frame_constant.light_color * frame_constant.light_intensity) * invd * (_diffuse * max(dot(vN, vL), 0.0f) + _specular * pow(max(dot(vH, vN), 0.0f), material_constant.shininess));

    return float4(vLightInts, 1.0f);
}
