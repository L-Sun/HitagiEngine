#include "bindless.hlsl"

struct BindlessInfo {
    hitagi::Texture gbuffer_albedo;
    hitagi::Texture gbuffer_normal;
    hitagi::Texture gbuffer_material;
    hitagi::Texture gbuffer_emissive;
    hitagi::Sampler sampler;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOutput VSMain(uint vertex_id : SV_VertexID) {
    const float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f),
    };
    const float2 uvs[3] = {
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f),
    };

    VSOutput output;
    output.pos = float4(positions[vertex_id], 0.0f, 1.0f);
    output.uv  = uvs[vertex_id];
    return output;
}

float4 PSAlbedoMain(VSOutput input) : SV_TARGET {
    BindlessInfo resource = hitagi::load_bindless<BindlessInfo>();
    SamplerState sampler  = resource.sampler.load();

    const float4 albedo = resource.gbuffer_albedo.sample_level<float4>(sampler, input.uv, 0.0f);
    return float4(albedo.rgb, 1.0f);
}

float4 PSNormalMain(VSOutput input) : SV_TARGET {
    BindlessInfo resource = hitagi::load_bindless<BindlessInfo>();
    SamplerState sampler  = resource.sampler.load();

    const float4 normal = resource.gbuffer_normal.sample_level<float4>(sampler, input.uv, 0.0f);
    return float4(normal.rgb, 1.0f);
}

float4 PSMaterialMain(VSOutput input) : SV_TARGET {
    BindlessInfo resource = hitagi::load_bindless<BindlessInfo>();
    SamplerState sampler  = resource.sampler.load();

    const float4 material = resource.gbuffer_material.sample_level<float4>(sampler, input.uv, 0.0f);
    return float4(material.rgb, 1.0f);
}

float4 PSEmissiveMain(VSOutput input) : SV_TARGET {
    BindlessInfo resource = hitagi::load_bindless<BindlessInfo>();
    SamplerState sampler  = resource.sampler.load();

    const float3 emissive = resource.gbuffer_emissive.sample_level<float3>(sampler, input.uv, 0.0f);
    return float4(saturate(emissive), 1.0f);
}
