cbuffer FrameConstants : register(b0) {
  matrix projView;
  matrix view;
  matrix projection;
  matrix invProjection;
  matrix invView;
  matrix invProjView;
  float4 cameraPos;
  float4 lightPosition;
  float4 lightPosInView;
  float4 lightIntensity;
};

cbuffer ObjectConstants : register(b1) {
  matrix model;
  float4 ambient;
  float4 diffuse;
  float4 emission;
  float4 specular;
  float specularPower;
};

Texture2D<float4> materialTexutrues[4] : register(t0);
sampler baseSampler : register(s0);

struct VSInput {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float2 uv : TEXCOORD;
};

struct PSInput {
  float4 position : SV_POSITION;
  float4 posInView : POSITION;
  float4 normal : NORMAL;
  float2 uv : TEXCOORD0;
};

#define RSDEF                                                                  \
  "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)"                              \
  "DescriptorTable(CBV(b0, numDescriptors = 2), SRV(t0, numDescriptors=4)),"   \
  "StaticSampler(s0)"

[RootSignature(RSDEF)] 
PSInput VSMain(VSInput input) {
  PSInput output;

  matrix MVP = mul(projView, model);
  output.position = mul(MVP, float4(input.position, 1.0f));
  output.normal = mul(view, mul(model, float4(input.normal, 0.0f)));
  output.uv = input.uv;
  output.posInView = mul(view, mul(model, float4(input.position, 1.0f)));
  return output;
}

[RootSignature(RSDEF)] 
float4 PSMain(PSInput input)
    : SV_TARGET {
  const float4 vN = normalize(input.normal);
  const float4 vL = normalize(lightPosInView - input.posInView);
  const float4 vV = normalize(-input.posInView);
  const float4 vH = normalize(vL + vV);
  const float r = length(lightPosInView - input.posInView);
  const float invd = 1.0f / (r * r + 1.0f);
  // color
  const float4 _ambient =
      (ambient.r < 0 ? materialTexutrues[0].Sample(baseSampler, input.uv)
                     : ambient);
  const float4 _diffuse =
      (diffuse.r < 0 ? materialTexutrues[1].Sample(baseSampler, input.uv)
                     : diffuse);
  const float4 _specular =
      (specular.r < 0 ? materialTexutrues[2].Sample(baseSampler, input.uv)
                      : specular);
  const float4 _specularPower =
      (specularPower < 0 ? materialTexutrues[3].Sample(baseSampler, input.uv)
                         : specularPower);

  float4 vLightInts =
      _ambient + (lightIntensity)*invd *
                     (_diffuse * max(dot(vN, vL), 0.0f) +
                      _specular * pow(max(dot(vH, vN), 0.0f), _specularPower));

  return vLightInts;
}
