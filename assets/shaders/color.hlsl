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
  float4 light_intensity;
};

cbuffer ObjectConstants : register(b1) {
  matrix model;
};

cbuffer MaterialConstants : register(b2) {
  float4 ambient;
  float4 diffuse;
  float4 emission;
  float4 specular;
  float specularPower;
}

// Texture2D<float4> materialTexutrues[4] : register(t0);
sampler baseSampler : register(s0);

struct VSInput {
  float3 position : POSITION;
  float4 color : COLOR;
  float3 normal : NORMAL;
  float2 uv : TEXCOORD;
};

struct PSInput {
  float4 position : SV_POSITION;
  float4 color : COLOR;
  float4 pos_in_view : POSITION;
  float4 normal : NORMAL;
  float2 uv : TEXCOORD0;
};

#define RSDEF                                                                  \
  "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT),"                             \
  "CBV(b0),"                                                                   \
  "CBV(b1),"                                                                   \
  "CBV(b2),"                                                                   \
  "StaticSampler(s0)"

[RootSignature(RSDEF)] 
PSInput VSMain(VSInput input) {
  PSInput output;

  matrix MVP = mul(proj_view, model);
  output.position = mul(MVP, float4(input.position, 1.0f));
  output.normal = mul(view, mul(model, float4(input.normal, 0.0f)));
  output.uv = input.uv;
  output.pos_in_view = mul(view, mul(model, float4(input.position, 1.0f)));
  output.color = input.color;
  return output;
}

[RootSignature(RSDEF)] 
float4 PSMain(PSInput input) : SV_TARGET {
  const float4 vN = normalize(input.normal);
  const float4 vL = normalize(light_pos_in_view - input.pos_in_view);
  const float4 vV = normalize(-input.pos_in_view);
  const float4 vH = normalize(vL + vV);
  const float r = length(light_pos_in_view - input.pos_in_view);
  const float invd = 1.0f / (r * r + 1.0f);
  // color
  // const float4 _ambient =
  //     (ambient.r < 0 ? materialTexutrues[0].Sample(baseSampler, input.uv)
  //                    : ambient);
  // const float4 _diffuse =
  //     (diffuse.r < 0 ? materialTexutrues[1].Sample(baseSampler, input.uv)
  //                    : diffuse);
  // const float4 _specular =
  //     (specular.r < 0 ? materialTexutrues[2].Sample(baseSampler, input.uv)
  //                     : specular);
  // const float4 _specularPower =
  //     (specularPower < 0 ? materialTexutrues[3].Sample(baseSampler, input.uv)
  //                        : specularPower);

  // float4 vLightInts =
  //     _ambient + (lightIntensity)*invd *
  //                    (_diffuse * max(dot(vN, vL), 0.0f) +
  //                     _specular * pow(max(dot(vH, vN), 0.0f), _specularPower));

  return diffuse;
}
