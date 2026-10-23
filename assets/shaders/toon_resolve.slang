import bindless;
struct ResolveBindings { hitagi::Texture color; hitagi::Texture depth; };
struct VertexOutput { float4 position : SV_Position; };
VertexOutput VSMain(uint id : SV_VertexID) {
    VertexOutput output;
    output.position = float4(id == 1 ? 3.0 : -1.0, id == 2 ? 3.0 : -1.0, 0.0, 1.0);
    return output;
}
struct PixelOutput { float4 color : SV_Target; float depth : SV_Depth; };
PixelOutput PSMain(VertexOutput input) {
    const ResolveBindings r = hitagi::load_bindless<ResolveBindings>();
    const uint2 origin = uint2(input.position.xy) * 2;
    PixelOutput output;
    output.color = float4(0,0,0,0);
    output.depth = 1.0;
    [unroll] for (uint y=0; y<2; ++y) {
        [unroll] for (uint x=0; x<2; ++x) {
            output.color += r.color.load<float4>(origin+uint2(x,y))*0.25;
            output.depth = min(output.depth, r.depth.load<float>(origin+uint2(x,y)));
        }
    }
    return output;
}
