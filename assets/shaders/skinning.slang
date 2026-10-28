import bindless;

struct SkinArguments {
    hitagi::SimpleBuffer inputs[4];
    hitagi::RWSimpleBuffer outputs[4];
    hitagi::SimpleBuffer indices;
    hitagi::SimpleBuffer weights;
    hitagi::SimpleBuffer palette;
    uint count;
    uint attributes;
};
struct JointMatrix { row_major float4x4 value; };

[numthreads(64, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID) {
    SkinArguments args = hitagi::load_bindless<SkinArguments>();
    uint v = thread.x;
    if (v >= args.count) return;
    uint4 indices = args.indices.load<uint4>(v * 16);
    float4 weights = args.weights.load<float4>(v * 16);
    // Scale first to keep the sum finite for any valid finite weights.
    float largest = max(max(weights.x, weights.y), max(weights.z, weights.w));
    float4x4 matrix = float4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
    if (largest > 0) {
        weights /= largest;
        weights /= dot(weights, float4(1));
        matrix = float4x4(0);
        for (uint k = 0; k < 4; ++k)
            if (weights[k] > 0) matrix += args.palette.load<JointMatrix>(indices[k] * 64).value * weights[k];
    }
    for (uint attribute = 0; attribute < 4; ++attribute) {
        if ((args.attributes & (1u << attribute)) == 0) continue;
        float3 value = args.inputs[attribute].load<float3>(v * 12);
        float3 result;
        if (attribute == 0) result = mul(matrix, float4(value, 1)).xyz;
        else {
            if (attribute == 1) {
                float3 x = float3(matrix[0][0], matrix[1][0], matrix[2][0]);
                float3 y = float3(matrix[0][1], matrix[1][1], matrix[2][1]);
                float3 z = float3(matrix[0][2], matrix[1][2], matrix[2][2]);
                float3 cx = cross(y, z);
                float determinant = dot(x, cx);
                result = abs(determinant) > 1e-8 ? (cx * value.x + cross(z, x) * value.y + cross(x, y) * value.z) / determinant : value;
            } else result = mul(matrix, float4(value, 0)).xyz;
            float magnitude = length(result);
            result = magnitude > 1e-8 ? result / magnitude : value;
        }
        args.outputs[attribute].store<float3>(result, v * 12);
    }
}
