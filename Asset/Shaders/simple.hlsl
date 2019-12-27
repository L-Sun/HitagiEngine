cbuffer FrameConstants : register(b0){
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
    float3 lightPosition;
    float4 lightColor;
};

cbuffer ObjectConstants : register(b1){
    matrix modelMatrix;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 normal   : NORMAL;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    matrix x = {
        {1.0f, 0.0f ,0.0f, 0.0f},
        {0.0f, 1.0f ,0.0f, 0.0f},
        {0.0f, 0.0f ,1.0f, 0.0f},
        {0.0f, 0.0f ,0.0f, 1.0f},
    };
    matrix trans = mul(projectionMatrix, mul(viewMatrix, mul(worldMatrix, modelMatrix)));
    output.position = mul(trans, float4(input.position, 1.0f));
    output.normal = mul(trans, float4(input.normal, 0.0f));

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(normalize(input.normal.xyz), 1.0f);
}
