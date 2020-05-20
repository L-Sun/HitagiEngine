cbuffer FrameConstants : register(b0){
    matrix porjView;
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

cbuffer ObjectConstants : register(b1){
    matrix model;
    float4 ambient;
    float4 diffuse;
    float4 emission;
    float4 specular;
    float  specularPower;
};

struct VSInput {
    float3 position : POSITION;
};

struct PSInput {
    float4 position : SV_POSITION;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    matrix MVP = mul(porjView, model);
    output.position = mul(MVP, float4(input.position, 1.0f));
    return  output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return diffuse;
}
