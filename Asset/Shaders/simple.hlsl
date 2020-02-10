cbuffer FrameConstants : register(b0){
    matrix WVP;
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
    float3 lightPos;
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
    float3 posInView :POSITION;
    float4 normal   : NORMAL;
    float2 uv : TEXCOORD0;
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

    matrix MVP = mul(WVP, modelMatrix);
    output.position = mul(MVP, float4(input.position, 1.0f));
    output.normal = normalize(mul(viewMatrix, mul(modelMatrix, float4(input.normal, 0.0f))));
    output.uv = input.uv;
    output.posInView = mul(viewMatrix, float4(input.position, 1.0f)).xyz;
    return  output;
}

float4 PSMain1(PSInput input) : SV_TARGET
{
	float3 lightRgb = lightColor.xyz;
    float3 lightPosInView = mul(viewMatrix, float4(lightPos,1.0f)).xyz;

	const float3 vN = normalize(input.normal.xyz);
	const float3 vL = normalize(lightPosInView.xyz - input.posInView.xyz);
    const float3 vR = normalize(2 * clamp(dot(vL, vN), 0.0f, 1.0f) * vN - vL);
	const float3 vV = normalize(input.posInView.xyz);
	float d = length(vL); 
	float3 vLightInts = float3(0.01f, 0.01f, 0.01f) + lightRgb * baseColor.xyz * clamp(dot(vN, vL), 0.0f, 1.0f)  + specularColor.xyz * pow(clamp(dot(vR,vV), 0.0f, 1.0f), specularPower);
	return float4(vLightInts, 1.0f);

}

float4 PSMain2(PSInput input) : SV_TARGET
{
	float3 lightRgb = lightColor.xyz;
    float3 lightPosInView = mul(viewMatrix, float4(lightPos,1.0f)).xyz;

	const float3 vN = normalize(input.normal.xyz);
	const float3 vL = normalize(lightPosInView.xyz - input.posInView.xyz);
    const float3 vR = normalize(2 * clamp(dot(vL, vN), 0.0f, 1.0f) * vN - vL);
	const float3 vV = normalize(input.posInView.xyz);
	float d = length(vL); 
    float3 vLightInts = float3(0.01f, 0.01f, 0.01f) 
							+ lightRgb * BaseMap.Sample(baseSampler, input.uv).xyz * clamp(dot(vN, vL), 0.0f, 1.0f) 
							+ specularColor.xyz * pow(clamp(dot(vR,vV), 0.0f, 1.0f), specularPower);

	return float4(vLightInts, 1.0f);

}
