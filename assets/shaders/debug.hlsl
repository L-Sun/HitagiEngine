cbuffer FrameConstants : register(b0) {
    matrix projView;
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

cbuffer ObjectConstants : register(b1)   {
    matrix transform;
    float4 color;
}

struct VSInput {
    float3 position : POSITION;
};

struct PSInput {
    float4 position : SV_POSITION;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    output.position = mul(projView, mul(transform, float4(input.position, 1.0f)));
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
	return color;
}
