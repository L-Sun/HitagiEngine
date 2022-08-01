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

#define RSDEF \
  "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), "  \
  "CBV(b0),"                                         \
  "CBV(b1)"                                         \

struct VSInput {
  float3 position : POSITION;
  float4 color : COLOR;
};

struct PSInput {
  float4 position : SV_POSITION;
  float4 color : COLOR;
};


[RootSignature(RSDEF)]
PSInput VSMain(VSInput input) {
  PSInput output;
  output.position = mul(proj_view, mul(world_transform, float4(input.position, 1.0f)));

  output.color = input.color;
  return output;
}

[RootSignature(RSDEF)] 
float4 PSMain(PSInput input) : SV_TARGET { return input.color; }
