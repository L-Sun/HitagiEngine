cbuffer vertexBuffer : register(b0) {
    matrix ProjectionMatrix; 
};

SamplerState sampler0 : register(s0);
Texture2D texture0 : register(t0);

struct VS_INPUT {
    float2 pos : POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};
            
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};
            
PS_INPUT VSMain(VS_INPUT input) {
    PS_INPUT output;
    output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));
    output.col = input.col;
    output.uv  = input.uv;
    return output;
}

 
float4 PSMain(PS_INPUT input) : SV_Target {
    return input.col *  texture0.Sample(sampler0, input.uv);
}