#define SamplesMSAA 4

Texture2DMS<float4, SamplesMSAA> msaaTexture : register(t0);
  
cbuffer msaaDesc : register(b0)
{
	uint2 dimensions;
}
  

struct VSInput {
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
	PSInput result;

	result.position = float4(input.position, 1.0f);
	result.uv = input.uv;

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	uint2 coord = uint2(input.uv.x * dimensions.x, input.uv.y * dimensions.y);
	
	float4 tex = float4(0.0f, 0.0f, 0.0f, 0.0f);
  
#if SamplesMSAA == 1
	tex = msaaTexture.Load(coord, 0);
#else
	for (uint i = 0; i < SamplesMSAA; i++)
	{
		tex += msaaTexture.Load(coord, i);
	}
	tex *= 1.0f / SamplesMSAA;
#endif
		
	return tex;
}