cbuffer FrameConstants : register(b0){
    matrix WVP;
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
    float4 lightPos;
    float4 lightPosInView;
    float4 lightColor;
    float  lightIntensity;
};

cbuffer ObjectConstants : register(b1){
    matrix modelMatrix;
    float4 ambientColor;
    float4 baseColor;
    float4 specularColor;
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
    float4 posInView :POSITION;
    float4 normal   : NORMAL;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;

    matrix MVP = mul(WVP, modelMatrix);
    output.position = mul(MVP, float4(input.position, 1.0f));
    output.normal = mul(viewMatrix, mul(modelMatrix, float4(input.normal, 0.0f)));
    output.uv = input.uv;
    output.posInView = mul(viewMatrix, mul(modelMatrix, float4(input.position, 1.0f)));
    return  output;
}

float4 PSMain1(PSInput input) : SV_TARGET
{
	const float4 vN = normalize(input.normal);
	const float4 vL = normalize(lightPosInView - input.posInView);
	const float4 vV = normalize(-input.posInView);
    const float4 vH = normalize(0.5 * (vL + vV));
    const float invd = 1.0f / length(lightPosInView - input.posInView);
	float4 vLightInts = ambientColor + lightColor * lightIntensity * invd * invd * (
                              baseColor * max(dot(vN, vL), 0.0f)
                            + specularColor * pow(max(dot(vH, vN), 0.0f), specularPower)
                        );

	return vLightInts;
}

float4 PSMain2(PSInput input) : SV_TARGET
{
	const float3 vN = normalize(input.normal.xyz);
	const float3 vL = normalize(lightPosInView.xyz - input.posInView.xyz);
	const float3 vV = normalize(-input.posInView.xyz);
    const float3 vH = normalize(0.5 * (vL + vV));
    const float invd = 1.0f / length(lightPosInView - input.posInView);
    float4 vLightInts = ambientColor + lightColor * lightIntensity * invd * invd * (
                              ambientColor
							+ BaseMap.Sample(baseSampler, input.uv) * max(dot(vN, vL), 0.0f) 
							+ specularColor * pow(max(dot(vH, vN), 0.0f), specularPower)
                        );

	return vLightInts;

}
