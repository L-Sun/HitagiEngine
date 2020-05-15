#define MAX_DEPTH 8
#define PI        3.14159265359
#define INV_PI    0.31830988618

struct RayPayload
{
    float4 color;
    int depth;
    int hitID;
    float T;
    bool isEmission;
};
struct Attributes {
  float2 bary;
};
struct Ray {
    float3 origin;
    float3 direction;
};

// Output 
RWTexture2D<float4> gOutput : register(u0, space0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0, space0);
StructuredBuffer<float> RandomNumbers : register(t1, space0);

cbuffer FrameConstants : register(b0, space0){
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
    uint   frameCount;
    int reset;
};


cbuffer ObjectConstants : register(b0, space1){
    matrix model;
    float4 ambient;
    float4 diffuse;
    float4 emission;
    float4 specular;
    float specularPower;
};

StructuredBuffer<float3> normals : register(t0, space1);
StructuredBuffer<int> indices : register(t1, space1);

float rand(float2 xy) {
    int index = frac(sin(dot(xy, float2(12.9898,78.233))) * 43758.5453) * 10000;
    return RandomNumbers[(index+frameCount)%10000];
}

float3 sampleInHemisphere(float2 seed, float3 N, float3 inRay) {
    float x_1 = rand(seed);
    float x_2 = rand(seed + 0.2);
    float z = abs(1.0f - 2.0f * x_1);
    float r = sqrt(1.0f - z * z);
    float phi = 2 * 3.1415926 * x_2;

    float3 left = normalize(cross(inRay, N));
    float3 front = normalize(cross(left, N));
    return  (r * cos(phi)) * left + (r * sin(phi)) * front + z * N;
}

void RecursiveTraceRay(in Ray ray, inout RayPayload payload) {
    if(payload.depth >= MAX_DEPTH) {
        payload.color = float4(0, 0, 0, 1);
        return;
    }
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;

    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES , ~0, 0, 0, 0, rayDesc, payload);
}



[shader("raygeneration")]
void RayGen() {
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 screenPos = ((launchIndex.xy + 0.5f) / dims.xy * 2.f - 1.f);
    // Define a ray, consisting of origin, direction, and the min-max distance
    // values
    screenPos.y = -screenPos.y;
    float4 posInWorld = mul(invProjection, float4(screenPos, 0, 1));
    posInWorld = posInWorld / posInWorld.w;
    posInWorld = mul(invView, posInWorld);
    
    Ray ray;
    // Transform view space origin to world space
    ray.origin = cameraPos.xyz;
    // Transform screen space pixel center to view space
    // then transform to world space
    ray.direction = normalize(posInWorld - cameraPos).xyz;

    RayPayload payload = {float4(0, 0, 0, 1), 0, 0, 0, false};
    RecursiveTraceRay(ray, payload);
    if (reset == 0) {
        gOutput[launchIndex] += (payload.color - gOutput[launchIndex]) / frameCount;
    } else {
        gOutput[launchIndex] = payload.color;
    }
    
    gOutput[launchIndex].a = 1;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, Attributes attr) {
    // just test intersection
    if(payload.hitID < 0) {
        payload.hitID = InstanceID();
        return;
    }
    if(emission.r != 0 || emission.g != 0 || emission.b != 0) {
        payload.color = emission;
        payload.isEmission = true;
        return;
    }
    
    // Get normol
    float3 barycentrics = float3(1 - attr.bary.x - attr.bary.y, attr.bary.x, attr.bary.y);
    uint vertId = 3 * PrimitiveIndex();
    float3 N = normals[indices[vertId + 0]] * barycentrics.x +
               normals[indices[vertId + 1]] * barycentrics.y +
               normals[indices[vertId + 2]] * barycentrics.z;

    N = normalize(mul(model, float4(N, 0)).xyz);

    // Hit point
    float3 p = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 wi = normalize(WorldRayDirection());
    float4 fr = diffuse * INV_PI;
    float4 dir_light = {0, 0, 0, 1};
    float4 indir_light = {0, 0, 0, 1};

    // Calculate directly lighting
    {
        const float2 lightPlanSize = {0.65, 0.525};
        const float  lightArea     = lightPlanSize.x * lightPlanSize.y;
        const float3 lightNormal   = {0, 0 , -1};
        
        // sample a vector in hemisphere, then calculate the light vector contributing to the hit the point
        float3 lightPoint = float3(
            lightPosition.x + (rand(p.zx) - 0.5) * lightPlanSize.x, 
            lightPosition.y + (rand(p.xz+27) - 0.5) * lightPlanSize.y,
            lightPosition.z - 0.01
        );

        float  r          = length(lightPoint - p);
        float3 ws         = normalize(lightPoint - p);
        if(dot(ws, N) > 0) {
            Ray toLight = {lightPoint, -ws};
            payload.hitID = -1;
            RecursiveTraceRay(toLight, payload);
            // No object in the middle between hit point and light
            if(payload.hitID == InstanceID()) {
                // Calculate the radiation
                dir_light = fr * lightIntensity * 100 * dot(ws, N) *  max(0, dot(-ws, lightNormal)) / (r * r) * lightArea;
            }
        }
    }
 
    
    float3 wo = normalize(sampleInHemisphere(p.xz- 0.2, N, wi));
    Ray reflectRay;
    reflectRay.origin    = p;
    reflectRay.direction = wo;

    payload.depth++;
    RecursiveTraceRay(reflectRay, payload);
    payload.depth--;
    if(!payload.isEmission) {
        indir_light = fr * payload.color * dot(wo, N) * (2 * PI);
    }

    payload.color = dir_light + indir_light;
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    payload.color = float4(0, 0, 0, 1);
}