cbuffer FrameConstants : register(b0) {
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

cbuffer ObjectConstants : register(b1) {
  matrix model;
};

cbuffer MaterialConstants : register(b2) {
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

Texture2D<float4> materialTexutrues[4] : register(t0);
sampler baseSampler : register(s0);

struct VSInput {
  float3 position : POSITION;
  float4 color    : COLOR;
  float3 normal   : NORMAL;
  float2 uv       : TEXCOORD;
};

struct PSInput {
  float4 position    : SV_POSITION;
  float4 color       : COLOR;
  float3 pos_in_view : POSITION;
  float3 normal      : NORMAL;
  float2 uv          : TEXCOORD0;
};

#define RSDEF                                                                  \
  "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT),"                             \
  "CBV(b0),"                                                                   \
  "CBV(b1),"                                                                   \
  "CBV(b2),"                                                                   \
  "DescriptorTable(SRV(t0, numDescriptors=4)),"                                \
  "StaticSampler(s0)"

[RootSignature(RSDEF)] 
PSInput VSMain(VSInput input) {
  PSInput output;

  matrix MVP = mul(proj_view, model);

  output.position    = mul( MVP, float4(input.position, 1.0f));
  output.normal      = mul(view, mul(model, float4(input.normal,   0.0f))).xyz;
  output.pos_in_view = mul(view, mul(model, float4(input.position, 1.0f))).xyz;
  output.color       = input.color;
  output.uv          = input.uv;
  return output;
}

[RootSignature(RSDEF)] 
float4 PSMain(PSInput input) : SV_TARGET {
  const float3  vN = normalize( input.normal);
  const float3  vL = normalize( light_pos_in_view.xyz - input.pos_in_view);
  const float3  vV = normalize(-input.pos_in_view);
  const float3  vH = normalize( vL + vV);
  const float    r =    length( light_pos_in_view.xyz - input.pos_in_view);
  const float invd = 1.0f / (r * r + 1.0f);
  // color
  const float3 _diffuse  = diffuse_texture  == -1 ?  diffuse  : materialTexutrues[ diffuse_texture].Sample(baseSampler, input.uv).xyz;
  const float3 _specular = specular_texture == -1 ? specular  : materialTexutrues[specular_texture].Sample(baseSampler, input.uv).xyz;
  const float3 _ambient  = ambient_texture  == -1 ?  ambient  : materialTexutrues[ ambient_texture].Sample(baseSampler, input.uv).xyz;
  const float3 _emissive = emissive_texture == -1 ? emissive  : materialTexutrues[emissive_texture].Sample(baseSampler, input.uv).xyz;

  float3 vLightInts = _ambient + (light_color * light_intensity) * invd * (_diffuse * max(dot(vN, vL), 0.0f) + _specular * pow(max(dot(vH, vN), 0.0f), shininess));

  return float4(vLightInts, 1.0f);
}
