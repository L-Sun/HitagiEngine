cbuffer FrameConstants : register(b0) {
  matrix proj_view;
};

cbuffer ObjectConstants : register(b1){
  matrix transform;
  float4 color;
};


struct VSInput {
  float3 position : POSITION;
};

struct PSInput {
  float4 position : SV_POSITION;
};


PSInput VSMain(VSInput input) {
  PSInput output;
  output.position = mul(proj_view, mul(transform, float4(input.position, 1.0f)));

  return output;
}

float4 PSMain(PSInput input) : SV_TARGET { return color; }
