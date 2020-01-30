cbuffer FrameConstants : register(b0){
    float3 lightPosition;
    float4 lightColor;
};

cbuffer ObjectConstants : register(b1){
    matrix MVP;
    float4 baseColor;
    float4 specularColor;
    float  specularPower;
};

struct VSInput {
    float3 position : POSITION;
    float3 color : COLOR;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput output;

    output.position = mul(MVP, float4(input.position, 1.0f));
    output.color = input.color;
    return  output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return float4(input.color, 1.0f);
}
