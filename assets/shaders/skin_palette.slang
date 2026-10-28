import bindless;
struct JointMatrix { row_major float4x4 value; };
struct PaletteArguments {
    row_major float4x4 world_to_mesh;
    hitagi::SimpleBuffer worlds;
    hitagi::SimpleBuffer inverse_bind;
    hitagi::RWSimpleBuffer output;
    uint count;
};
[numthreads(64, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID) {
    PaletteArguments args = hitagi::load_bindless<PaletteArguments>();
    uint i = thread.x;
    if (i >= args.count) return;
    JointMatrix result;
    result.value = mul(mul(args.world_to_mesh, args.worlds.load<JointMatrix>(i * 64).value),
                       args.inverse_bind.load<JointMatrix>(i * 64).value);
    args.output.store<JointMatrix>(result, i * 64);
}
