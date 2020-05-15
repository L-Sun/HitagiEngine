#define MAX_DEPTH 8
#define PI        3.14159265359
#define _2PI      6.28318530718
#define INV_PI    0.31830988618

#define factor_kappa  0.3
#define factor_sigma  0.005

struct RayPayload
{
    float4 color;
    int    depth;
    float  testT;
    float  T;
    bool   isEmission;
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
    float z = x_1;
    float r = sqrt(1.0f - z * z);
    float phi = _2PI * x_2;

    float3 left = normalize(cross(inRay, N));
    float3 front = normalize(cross(left, N));
    return (r * cos(phi)) * left + (r * sin(phi)) * front + z * N;
}

float3 sampleInSphere(float2 seed) {
    float x_1 = rand(seed + 0.3);
    float x_2 = rand(seed + 0.5);
    float z = 1.0f - 2.0f * x_1;
    float r = sqrt(1.0f - z * z);
    float phi = _2PI * x_2;

    return float3(r * cos(phi), r * sin(phi), z);
}

float PossionRand(float2 seed, float k) {
    // pdf k*exp(-kx)
    float x = rand(seed);
    return - log(x) / k;
}

float InvPossionPDF(float x, float k) {
    return exp(k * x) / k;
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

    RayPayload payload;
    payload.color      = float4(0, 0, 0, 1);
    payload.depth      = 0;
    payload.testT      = 0;
    payload.T          = PossionRand(screenPos, factor_kappa);
    payload.isEmission = false;
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
    // just test interscetion
    if(payload.testT < 0) {
        payload.testT = RayTCurrent();
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
    bool HitSurface = payload.T >= RayTCurrent();
    if(HitSurface && (emission.r != 0 || emission.g != 0 || emission.b != 0)) {
        payload.isEmission = true;
        payload.color = lightIntensity * exp(factor_kappa * RayTCurrent());
        return;
    }

    float3 p = WorldRayOrigin() + payload.T * WorldRayDirection();
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
            lightPosition.x + (rand(p.xz) - 0.5) * lightPlanSize.x, 
            lightPosition.y + (rand(p.xz+27) - 0.5) * lightPlanSize.y,
            lightPosition.z - 0.01
        );

        float r          = length(lightPoint - p);
        float3 ws        = normalize(lightPoint - p);
        Ray toLight = {p, ws};
        payload.testT = -1;
        payload.depth++;
        RecursiveTraceRay(toLight, payload);
        payload.depth--;
        // No object in the middle between hit point and light
        if(payload.testT > r) {
            // Calculate the radiation
            if(HitSurface) {
                dir_light =   fr 
                            * lightIntensity * 100 * exp(-factor_kappa * r)
                            * max(0, dot(ws, N))
                            * max(0, dot(-ws, lightNormal)) 
                            / (r * r) 
                            * lightArea;
            } else {
                dir_light =  factor_sigma / factor_kappa
                            * 3 * 0.0625 * (1 + dot(wi, ws) * dot(wi, ws))
                            * lightIntensity * 100 * exp(-factor_kappa * r)
                            * max(0, dot(-ws, lightNormal)) 
                            / (r * r) 
                            * lightArea;
            }
        }
    }
 
    
    float3 wo = HitSurface ? sampleInHemisphere(p.xz - 0.2, N, wi)
                           : sampleInSphere(p.xz - 0.2);
    Ray reflectRay;
    reflectRay.origin    = p;
    reflectRay.direction = wo;
    
    float t = payload.T;
    payload.T = PossionRand(p.xy, factor_kappa);
    payload.depth++;
    RecursiveTraceRay(reflectRay, payload);
    payload.depth--;
    payload.T = t;

    if(!payload.isEmission) {
        if(HitSurface) {
            indir_light = fr * payload.color * dot(wo, N) * (_2PI);
        }
        else {
            indir_light = factor_sigma / factor_kappa 
                        * 3 * 0.0625 * (1 + dot(wi, wo) * dot(wi, wo))  
                        * payload.color 
                        * (4 * PI);
        }
    }

    payload.color = (dir_light + indir_light) * (HitSurface ?
                                                              exp(factor_kappa * RayTCurrent())
                                                            : InvPossionPDF(payload.T, factor_kappa)
                                                );
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    payload.testT = RayTCurrent();
    payload.color = float4(0, 0, 0, 1);
}