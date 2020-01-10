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

Texture2D BaseMap : register(t0);
sampler baseSampler : register(s0);

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 normal   : NORMAL;
    float2 uv : TEXCOORD0;
    float3 posInView : TEXCOORD1;
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
    output.normal = normalize(mul(trans, float4(input.normal, 0.0f)));
    output.posInView = output.position.xyz;
    output.uv = input.uv;
    return  output;
}

float4 PSMain1(PSInput input) : SV_TARGET
{
	float3 lightRgb = lightColor.xyz;

	const float3 vN = normalize(input.normal.xyz);
	const float3 vL = normalize(lightPosition.xyz - input.posInView);
    const float3 vR = normalize(2 * dot(vL, vN) * vN - vL);
	const float3 vV = normalize(float3(0.0f,0.0f,0.0f) - input.posInView);
	float d = length(vL); 
	float3 vLightInts = float3(0.0f, 0.0f, 0.01f) + lightRgb * baseColor.xyz * dot(vN, vL) + specularColor.xyz * pow(clamp(dot(vR,vV), 0.0f, 1.0f), specularPower);
	return float4(vLightInts, 0.6f);

}

float4 PSMain2(PSInput input) : SV_TARGET
{
	float3 lightRgb = lightColor.xyz;

	const float3 vN = normalize(input.normal.xyz);
	const float3 vL = normalize(lightPosition.xyz - input.posInView);
    const float3 vR = normalize(2 * dot(vL, vN) * vN - vL);
	const float3 vV = normalize(float3(0.0f,0.0f,0.0f) - input.posInView);
	float d = length(vL); 
    float3 vLightInts = float3(0.0f, 0.0f, 0.01f) 
							+ lightRgb * BaseMap.Sample(baseSampler, input.uv).xyz * clamp(dot(vN, vL), 0.0f, 1.0f) 
							+ specularColor.xyz * pow(clamp(dot(vR,vV), 0.0f, 1.0f), specularPower);

	return float4(vLightInts, 1.0f);

}
