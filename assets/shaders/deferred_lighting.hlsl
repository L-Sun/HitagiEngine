#include "bindless.hlsl"

static const uint  MAX_DEFERRED_LIGHTS = 32;
static const float PI = 3.14159265359f;

struct BindlessInfo {
    hitagi::SimpleBuffer frame_constant;
    hitagi::Texture      gbuffer_albedo;
    hitagi::Texture      gbuffer_normal;
    hitagi::Texture      gbuffer_material;
    hitagi::Texture      gbuffer_emissive;
    hitagi::Sampler      sampler;
};

struct DeferredLight {
    float4 position_in_view;
    float4 color_intensity;
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
    uint   light_count;
    float  ambient_intensity;
    float  exposure;
    float  ssao_strength;
    float4 viewport;
    DeferredLight lights[MAX_DEFERRED_LIGHTS];
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(uint vertex_id : SV_VertexID) {
    static const float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f),
    };

    const float2 position = positions[vertex_id];

    PSInput output;
    output.position = float4(position, 0.0f, 1.0f);
    output.uv       = float2(position.x * 0.5f + 0.5f, 0.5f - position.y * 0.5f);
    return output;
}

float3 ReconstructViewPosition(float2 uv, float linear_depth, matrix inv_projection) {
    const float2 ndc      = float2(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f);
    float4       view_far = mul(inv_projection, float4(ndc, 1.0f, 1.0f));
    view_far /= max(abs(view_far.w), 0.00001f);

    const float3 ray = normalize(view_far.xyz);
    return ray * (linear_depth / max(-ray.z, 0.00001f));
}

float3 ACESFilm(float3 color) {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float3 SkyColor(float2 uv, FrameConstant frame_constant) {
    const float2 ndc      = float2(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f);
    float4       view_far = mul(frame_constant.inv_projection, float4(ndc, 1.0f, 1.0f));
    view_far /= max(abs(view_far.w), 0.00001f);

    const float3 view_ray = normalize(view_far.xyz);
    const float  horizon  = saturate(view_ray.y * 0.5f + 0.5f);
    return lerp(float3(0.012f, 0.014f, 0.018f), float3(0.16f, 0.19f, 0.23f), horizon);
}

float DistributionGGX(float3 normal, float3 half_dir, float roughness) {
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float n_dot_h = saturate(dot(normal, half_dir));
    const float n_dot_h2 = n_dot_h * n_dot_h;
    const float denom = n_dot_h2 * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * denom * denom, 0.000001f);
}

float GeometrySchlickGGX(float n_dot_v, float roughness) {
    const float r = roughness + 1.0f;
    const float k = (r * r) / 8.0f;
    return n_dot_v / max(n_dot_v * (1.0f - k) + k, 0.000001f);
}

float GeometrySmith(float3 normal, float3 view_dir, float3 light_dir, float roughness) {
    const float n_dot_v = saturate(dot(normal, view_dir));
    const float n_dot_l = saturate(dot(normal, light_dir));
    return GeometrySchlickGGX(n_dot_v, roughness) * GeometrySchlickGGX(n_dot_l, roughness);
}

float3 FresnelSchlick(float cos_theta, float3 f0) {
    return f0 + (1.0f - f0) * pow(saturate(1.0f - cos_theta), 5.0f);
}

float ComputeScreenSpaceAO(BindlessInfo resource, SamplerState sampler, FrameConstant frame_constant, float2 uv, float3 position_in_view, float3 normal_in_view, float linear_depth) {
    static const float2 offsets[12] = {
        float2(1.0f, 0.0f),
        float2(-1.0f, 0.0f),
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(0.707f, 0.707f),
        float2(-0.707f, 0.707f),
        float2(0.707f, -0.707f),
        float2(-0.707f, -0.707f),
        float2(0.382f, 0.924f),
        float2(-0.924f, 0.382f),
        float2(0.924f, -0.382f),
        float2(-0.382f, -0.924f),
    };

    const float2 texel_size   = frame_constant.viewport.zw;
    const float  view_radius  = clamp(linear_depth * 0.06f, 25.0f, 260.0f);
    const float  pixel_radius = clamp(18.0f / max(linear_depth * 0.0015f, 0.8f), 3.0f, 18.0f);

    float occlusion = 0.0f;
    float samples   = 0.0f;
    for (uint i = 0; i < 12; ++i) {
        const float  sample_scale = 0.45f + 0.55f * (float(i % 4) / 3.0f);
        const float2 sample_uv    = uv + offsets[i] * texel_size * pixel_radius * sample_scale;

        if (sample_uv.x <= 0.0f || sample_uv.y <= 0.0f || sample_uv.x >= 1.0f || sample_uv.y >= 1.0f) {
            continue;
        }

        const float sample_depth = resource.gbuffer_albedo.sample_level<float4>(sampler, sample_uv, 0.0f).a;
        if (sample_depth <= 0.00001f) {
            continue;
        }

        const float3 sample_position = ReconstructViewPosition(sample_uv, sample_depth, frame_constant.inv_projection);
        const float3 to_sample       = sample_position - position_in_view;
        const float  distance        = length(to_sample);
        const float  depth_delta     = linear_depth - sample_depth;
        const float  directional     = saturate(dot(normal_in_view, normalize(to_sample)) - 0.05f);
        const float  range_weight    = saturate(1.0f - distance / view_radius);
        const float  depth_weight    = smoothstep(0.5f, view_radius * 0.45f, depth_delta);

        occlusion += directional * range_weight * depth_weight;
        samples += 1.0f;
    }

    if (samples <= 0.0f) {
        return 1.0f;
    }

    const float ao = 1.0f - frame_constant.ssao_strength * (occlusion / samples) * 2.25f;
    return saturate(ao * 0.9f + 0.1f);
}

float4 PSMain(PSInput input) : SV_TARGET {
    const BindlessInfo  resource       = hitagi::load_bindless<BindlessInfo>();
    const FrameConstant frame_constant = resource.frame_constant.load<FrameConstant>();
    const SamplerState  sampler        = resource.sampler.load();
    const float4        albedo_depth   = resource.gbuffer_albedo.sample_level<float4>(sampler, input.uv, 0.0f);
    const float4        normal_data    = resource.gbuffer_normal.sample_level<float4>(sampler, input.uv, 0.0f);
    const float4        material_data  = resource.gbuffer_material.sample_level<float4>(sampler, input.uv, 0.0f);
    const float3        emissive       = resource.gbuffer_emissive.sample_level<float3>(sampler, input.uv, 0.0f);

    const float3 albedo       = saturate(albedo_depth.rgb);
    const float  linear_depth = albedo_depth.a;

    if (linear_depth <= 0.00001f) {
        float3 sky = ACESFilm(SkyColor(input.uv, frame_constant) * frame_constant.exposure);
        sky = pow(saturate(sky), 1.0f / 2.2f);
        return float4(sky, 1.0f);
    }

    const float3 position_in_view = ReconstructViewPosition(input.uv, linear_depth, frame_constant.inv_projection);
    float3       normal_in_view   = normalize(normal_data.rgb * 2.0f - 1.0f);
    const float3 view_dir = normalize(-position_in_view);
    if (dot(normal_in_view, view_dir) < 0.0f) {
        normal_in_view = -normal_in_view;
    }

    const float metallic      = saturate(material_data.r);
    const float roughness     = clamp(material_data.g, 0.04f, 1.0f);
    const float texture_ao    = saturate(normal_data.a);
    const float ssao          = ComputeScreenSpaceAO(resource, sampler, frame_constant, input.uv, position_in_view, normal_in_view, linear_depth);
    const float ao            = texture_ao * ssao;
    const float3 f0           = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    const float hemi             = saturate(normal_in_view.y * 0.5f + 0.5f);
    const float3 ambient_ground  = float3(0.055f, 0.052f, 0.048f);
    const float3 ambient_sky     = float3(0.22f, 0.25f, 0.29f);
    const float3 ambient_light   = lerp(ambient_ground, ambient_sky, hemi) * frame_constant.ambient_intensity;
    const float3 ambient_diffuse = albedo * (1.0f - metallic);
    const float3 ambient_specular = f0 * (0.18f + 0.62f * (1.0f - roughness));
    float3       color           = (ambient_diffuse + ambient_specular) * ambient_light * ao;

    const uint light_count = min(frame_constant.light_count, MAX_DEFERRED_LIGHTS);
    for (uint i = 0; i < light_count; ++i) {
        const float3 light_vector = frame_constant.lights[i].position_in_view.xyz - position_in_view;
        const float  distance2    = dot(light_vector, light_vector);
        const float3 light_dir    = normalize(light_vector);
        const float3 half_dir     = normalize(light_dir + view_dir);
        const float3 radiance     = frame_constant.lights[i].color_intensity.rgb * frame_constant.lights[i].color_intensity.a;
        const float  distance     = sqrt(distance2);
        const float  attenuation  = 1.0f / (1.0f + distance * 0.016f + distance2 * 0.00025f);
        const float  n_dot_l      = saturate(dot(normal_in_view, light_dir));
        const float  n_dot_v      = saturate(dot(normal_in_view, view_dir));
        const float  n_dot_h      = saturate(dot(normal_in_view, half_dir));

        const float  ndf      = DistributionGGX(normal_in_view, half_dir, roughness);
        const float  geometry = GeometrySmith(normal_in_view, view_dir, light_dir, roughness);
        const float3 fresnel  = FresnelSchlick(n_dot_h, f0);
        const float3 specular = (ndf * geometry * fresnel) / max(4.0f * n_dot_v * n_dot_l, 0.0001f);
        const float3 kd       = (1.0f - fresnel) * (1.0f - metallic);

        color += (kd * albedo / PI + specular) * radiance * attenuation * n_dot_l;
    }

    color = (color + emissive) * frame_constant.exposure;
    color = ACESFilm(color);
    color = pow(saturate(color), 1.0f / 2.2f);
    return float4(color, 1.0f);
}
