cbuffer FrameConstants : register(b0) {
  matrix proj_view;
  matrix view;
  matrix projection;
  matrix inv_projection;
  matrix inv_view;
  matrix inv_proj_view;
  float4 camera_pos;
  float4 light_position;
  float4 light_pos_in_view;
  float4 light_intensity;
};

cbuffer ObjectConstants : register(b1) {
  matrix world_transform;
};

cbuffer ImGuiConstants : register(b2) { matrix orth_projection; }

SamplerState sampler0 : register(s0);
Texture2D texture0 : register(t0);

#define RSDEF                                                                  \
  "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), "                            \
  "DescriptorTable(CBV(b0, numDescriptors = 3), SRV(t0)),"                     \
  "StaticSampler(s0)"

struct VS_INPUT {
  float3 pos : POSITION;
  float4 col : COLOR;
  float2 uv : TEXCOORD;
};

struct PS_INPUT {
  float4 pos : SV_POSITION;
  float4 col : COLOR;
  float2 uv : TEXCOORD;
};

[RootSignature(RSDEF)] PS_INPUT VSMain(VS_INPUT input) {
  PS_INPUT output;
  output.pos = mul(orth_projection, float4(input.pos.xyz, 1.f));
  output.col = input.col;
  output.uv = input.uv;
  return output;
}

    [RootSignature(RSDEF)] float4 PSMain(PS_INPUT input)
    : SV_TARGET {
  float4 out_col = input.col * texture0.Sample(sampler0, input.uv);
  return out_col;
}