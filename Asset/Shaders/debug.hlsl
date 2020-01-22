cbuffer FrameConstants : register(b0){
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
    float3 lightPosition;
    float4 lightColor;
};

cbuffer ObjectConstants : register(b1){
    matrix modelMatrix;
    float4  baseColor;
    float4  specularColor;
    float specularPower;
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

    matrix trans = mul(projectionMatrix, mul(viewMatrix, mul(worldMatrix, modelMatrix)));
    output.position = mul(trans, float4(input.position, 1.0f));
    output.color = input.color;
    return  output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return float4(input.color, 1.0f);
}
