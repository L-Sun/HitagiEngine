import bindless;

import anime_contract;

struct MeshInput { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; };
struct Varying {
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 world : TEXCOORD1;
    float3 local : TEXCOORD2;
};
Varying MeshVS(MeshInput input) {
    AnimeDrawBindings bindings = hitagi::load_bindless<AnimeDrawBindings>();
    AnimeInstanceData d = bindings.instance.load<AnimeInstanceData>(bindings.instance_index * bindings.instance_stride);
    Varying o;
    o.position = mul(d.mvp, float4(input.position, 1));
    o.normal = normalize(mul(d.normal_matrix, float4(input.normal, 0)).xyz);
    o.uv = input.uv;
    o.world = mul(d.model, float4(input.position, 1)).xyz;
    o.local = input.position;
    return o;
}
struct Screen { float4 position : SV_Position; float2 uv : TEXCOORD; };
Screen FullscreenVS(uint id : SV_VertexID) {
    Screen o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.position = float4(o.uv.x * 2 - 1, 1 - o.uv.y * 2, 0, 1);
    return o;
}
float3 unproject(AnimeFrameData d, float2 uv, float depth) {
    float4 p = mul(d.inverse_vp, float4(uv.x * 2 - 1, 1 - uv.y * 2, depth, 1));
    return p.xyz / p.w;
}
// Interpolate comparison results, not depth values. Sub-texel PCF removes the
// ten discrete visibility levels produced by nine nearest-neighbour tests.
float compareBilinear(hitagi::Texture shadow, float2 pixel, float receiver, int size) {
    int2 p = int2(floor(pixel));
    float2 f = frac(pixel);
    float a = receiver < shadow.load<float>(uint2(clamp(p,0,size-1))) ? 1 : 0;
    float b = receiver < shadow.load<float>(uint2(clamp(p+int2(1,0),0,size-1))) ? 1 : 0;
    float c = receiver < shadow.load<float>(uint2(clamp(p+int2(0,1),0,size-1))) ? 1 : 0;
    float e = receiver < shadow.load<float>(uint2(clamp(p+int2(1,1),0,size-1))) ? 1 : 0;
    return lerp(lerp(a,b,f.x),lerp(c,e,f.x),f.y);
}
float2 shadowDisk(int index, int count) {
    // Equal-area strata with a golden-angle rotation. Keep the pattern fixed
    // in light space: this viewer has no temporal accumulation to remove jitter.
    float radius = sqrt((index + 0.5) / count);
    float angle = index * 2.39996323;
    return radius * float2(cos(angle), sin(angle));
}
float shadowAt(AnimeFrameData d, hitagi::Texture shadow, float3 world, float3 normal) {
    if (d.options.x < 0.5) return 1;
    float slope = 1-saturate(dot(normal,d.light.xyz));
    float3 offset = normal * d.shadow_settings.y * (1.0+2.0*slope);
    float4 p = mul(d.shadow_mvp, float4(world+offset, 1));
    p.xyz /= p.w;
    float2 uv = float2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
    if (any(uv < 0) || any(uv > 1) || p.z < 0 || p.z > 1) return 1;
    int size = int(d.shadow_settings.x);
    float2 pixel = uv*size-0.5;
    float receiver = p.z-d.shadow_settings.z*(1+slope);
    // Small distant casters (e.g. an earpiece above a shoulder) expose sparse
    // PCSS sampling as blotches on plain skin. Search the full filter support
    // and integrate a denser disk instead of hiding the shadow with more bias.
    float blockers = 0, blockerDepth = 0;
    for (int i=0;i<32;++i) {
        int2 samplePixel = int2(round(pixel+shadowDisk(i,32)*9));
        float z = shadow.load<float>(uint2(clamp(samplePixel,0,size-1)));
        if (z < receiver) { blockerDepth += z; blockers += 1; }
    }
    if (blockers == 0) return 1;
    float separation = max(0,receiver-blockerDepth/blockers);
    float radius = clamp(1.5+separation*size*0.85,1.5,9);
    float visibility = 0;
    for (int i=0;i<64;++i)
        visibility += compareBilinear(shadow,pixel+shadowDisk(i,64)*radius,receiver,size);
    return visibility/64;
}

float3 materialColor(int kind) {
    if (kind == 1) return float3(1,0.7,0.4);
    if (kind == 2) return float3(1,0.3,0.5);
    if (kind == 3) return float3(0.1,0.8,1);
    if (kind == 4) return float3(0.7,0.4,1);
    return float3(0.4,0.7,0.4);
}

float4 MeshPS(Varying input) : SV_Target {
    AnimeDrawBindings bindings = hitagi::load_bindless<AnimeDrawBindings>();
    AnimeFrameData f = bindings.frame.load<AnimeFrameData>();
    AnimeInstanceData instance = bindings.instance.load<AnimeInstanceData>(bindings.instance_index * bindings.instance_stride);
    AnimeMaterialData m = bindings.material_data.load<AnimeMaterialData>(bindings.material_offset);
    AnimePassData p = bindings.pass_data.load<AnimePassData>();
    float4 color = m.base_color;
    if (m.textured > 0.5) color *= m.base_color_texture.sample<float4>(p.sampler.load(), input.uv);
    clip(color.a - m.alpha_cutoff);
    float3 n = normalize(input.normal);
    if (p.mode < 0.5) return float4(color.rgb, 1);
    if (p.mode < 1.5) return float4(n * 0.5 + 0.5, 1);
    if (p.mode < 2.5) return float4(float3(1.0 - input.position.z), 1);

    int kind = int(m.kind+0.5);
    if (p.mode > 3.5 && p.mode < 4.5) return float4(materialColor(kind),1);
    float3 v = normalize(f.eye.xyz-input.world);
    float NoL = dot(n,f.light.xyz);
    float transition = max(0.045, fwidth(NoL)*1.5);
    float band = smoothstep(-0.08-transition,-0.08+transition,NoL);
    float visibility = shadowAt(f,p.shadow,input.world,n);
    float3 dark = float3(0.64,0.68,0.80), lit = float3(1.06,1.04,1.02);
    if (kind == 1 || kind == 2) {
        dark = float3(0.86,0.68,0.68);
        lit = float3(1.16,1.16,1.14);
    }
    if (kind == 2 && f.options.y > 0.5) {
        // Assets without a control map use an analytic head-local threshold.
        // Neither this fallback nor generated maps recover miHoYo's face SDF.
        float3 right = normalize(mul(instance.model,float4(m.head_right.xyz,0)).xyz);
        float3 up = normalize(mul(instance.model,float4(m.head_up.xyz,0)).xyz);
        float3 forward = -normalize(cross(right,up));
        float3 horizontal = f.light.xyz-up*dot(up,f.light.xyz);
        horizontal /= max(length(horizontal),0.001);
        float angle = acos(clamp(dot(forward,horizontal),-1,1))/3.14159265;
        float3 relative = input.local-m.head.xyz;
        float2 face = float2(dot(relative,m.head_right.xyz),dot(relative,m.head_up.xyz))/max(m.head.w,0.001);
        float side = face.x * (dot(right,horizontal) >= 0 ? 1 : -1);
        float threshold = saturate(0.52+0.43*side+0.04*cos(face.y*3));
        float hairMask = smoothstep(-0.22,0.2,face.y);
        if ((m.controlled * f.options.w) > 0.5) {
            // Blender-authored head-planar data: R/G thresholds for opposing
            // light directions, B hair occlusion mask. Values are raw UNORM.
            float3 control = m.control_texture.sample<float4>(p.sampler.load(),saturate(face*0.5+0.5)).rgb;
            threshold = dot(right,horizontal) >= 0 ? control.r : control.g;
            hairMask = control.b;
        }
        band = smoothstep(angle-0.035,angle+0.035,threshold);
        visibility = shadowAt(f,p.hair_shadow,input.world,n);
        // Soften hair's occlusion on the face; keep a readable, warm shadow.
        visibility = lerp(1,lerp(0.60,1,visibility),hairMask);
    }
    if (kind == 4) {
        // Eye/eyelash materials avoid noisy eye-socket self shadows.
        band = smoothstep(-0.3,0.3,NoL);
        visibility = 1;
        dark = float3(0.98,0.98,1);
        lit = float3(1.18,1.18,1.18);
    }
    float lighting = min(band, visibility);
    float3 diffuse = color.rgb * lerp(dark,lit,lighting);
    float3 specular = 0;
    if (kind == 3) {
        // A broad view-dependent lobe shaped into a band. Head up provides the
        // fallback; an authored UV-space flow map can steer the strand direction.
        float3 up = normalize(mul(instance.model,float4(m.head_up.xyz,0)).xyz);
        float strength = 1;
        if ((m.controlled * f.options.w) > 0.5) {
            float4 control = m.control_texture.sample<float4>(p.sampler.load(),input.uv);
            float2 direction = control.rg*2-1;
            float3 dx = ddx(input.world), dy = ddy(input.world);
            float2 ux = ddx(input.uv), uy = ddy(input.uv);
            float determinant = ux.x*uy.y-ux.y*uy.x;
            if (abs(determinant) > 1e-10 && dot(direction,direction) > 0.001) {
                float3 t = (dx*uy.y-dy*ux.y)/determinant;
                float3 b = (dy*ux.x-dx*uy.x)/determinant;
                t = normalize(t-n*dot(t,n));
                b = normalize(cross(n,t))*(dot(cross(n,t),b) >= 0 ? 1 : -1);
                float3 flow = normalize(t*direction.x+b*direction.y);
                if (dot(flow,up) < 0) flow = -flow;
                up = normalize(lerp(up,flow,control.b));
            }
            strength = control.a;
        }
        float3 h = normalize(v+f.light.xyz);
        float3 tangent = up-n*dot(n,up);
        tangent /= max(length(tangent),0.001);
        float sine = sqrt(saturate(1-pow(dot(tangent,h),2)));
        float NoH = saturate(dot(n,h));
        float highlight = (pow(sine,20)*0.14+pow(sine,100)*0.08)*pow(NoH,6)*lerp(0.35,1,lighting);
        specular = lerp(color.rgb,float3(0.65,0.94,0.93),0.55)*highlight*strength;
    } else if (kind == 0) {
        float NoH = saturate(dot(n,normalize(v+f.light.xyz)));
        specular = float3(0.055)*smoothstep(0.78,0.95,pow(NoH,20))*lighting;
    }
    specular *= f.options.z;
    float rim = pow(1-saturate(dot(n,v)),5)*smoothstep(-0.15,0.5,NoL)*0.06;
    if (kind == 2 || kind == 4) rim *= 0.15;
    if (p.mode > 4.5 && p.mode < 5.5) return float4(float3(visibility),1);
    if (p.mode > 5.5 && p.mode < 6.5) return float4(float3(band),1);
    if (p.mode > 6.5) return float4(specular,1);
    return float4(saturate(diffuse+specular+rim*float3(0.65,0.8,1)),1);
}
float3 sky(float3 ray) {
    float h = saturate(ray.y * 1.6);
    float3 color = lerp(float3(0.78, 0.87, 0.91), float3(0.27, 0.60, 0.83), h);
    float clouds = sin(ray.x * 12 + ray.z * 4) * sin(ray.z * 17 - ray.x * 3);
    color = lerp(color, float3(0.97, 0.97, 0.93), smoothstep(0.28, 0.66, clouds) * smoothstep(0.03, 0.20, ray.y) * (1-h)*0.7);
    float hills = 0.045 + 0.025*sin(atan2(ray.z,ray.x)*7) + 0.018*sin(atan2(ray.z,ray.x)*17);
    if (ray.y < hills) color = float3(0.47,0.66,0.65);
    return color;
}
// Integrate actual geometry samples before filtering in output-pixel units.
float3 resolvedColor(AnimeFullscreenData d, SamplerState sampler, float2 uv) {
    if (d.scale < 1.5) return d.texture0.sample<float4>(sampler,uv).rgb;
    if (d.scale > 3.5) {
        // A box resolve retains too much high-frequency energy in distant
        // eyelashes and trim. Integrate a separable tent over the 4x raster.
        // The eight source weights [1,3,5,7,7,5,3,1]/32 on each axis combine
        // into four bilinear taps (64 source samples, 16 texture fetches).
        // Positive weights avoid ringing; support is one output pixel per side.
        const float positions[4] = {-0.6875,-0.229166667,0.229166667,0.6875};
        const float weights[4] = {0.125,0.375,0.375,0.125};
        float3 color = 0;
        for (int y=0;y<4;++y)
            for (int x=0;x<4;++x)
                color += d.texture0.sample<float4>(sampler,
                    uv+float2(positions[x],positions[y])/d.settings.yz).rgb*weights[x]*weights[y];
        return color;
    }
    // Scale=2: four texel-centre samples, followed by selective edge filtering.
    float2 offset = 0.25/d.settings.yz;
    return 0.25*(d.texture0.sample<float4>(sampler,uv+float2(-offset.x,-offset.y)).rgb
                +d.texture0.sample<float4>(sampler,uv+float2( offset.x,-offset.y)).rgb
                +d.texture0.sample<float4>(sampler,uv+float2(-offset.x, offset.y)).rgb
                +d.texture0.sample<float4>(sampler,uv+float2( offset.x, offset.y)).rgb);
}
float4 FullscreenPS(Screen input) : SV_Target {
    AnimeFullscreenData d = hitagi::load_bindless<AnimeFullscreenData>();
    AnimeFrameData f = d.frame.load<AnimeFrameData>();
    uint2 pixel = uint2(input.position.xy);
    float2 uv = input.position.xy / d.settings.yz;
    if (d.settings.x > 5.5) {
        // Contrast-adaptive edge filtering after composition and SSAA resolve.
        // Low-contrast areas retain their resolved color without spatial blur.
        SamplerState sampler = d.sampler.load();
        float2 texel = 1.0/d.settings.yz;
        float3 center = resolvedColor(d,sampler,uv);
        // The 4x reconstruction already filters geometry and texture details;
        // do not run the directional heuristic again over small text/eyelashes.
        if (d.scale > 3.5) return float4(center,1);
        if (d.scale > 1.5) {
            // SSAA already integrates texture detail. Restrict the extra filter
            // to geometry/normal edges so small lettering and eyes stay crisp.
            float4 surface = d.texture1.sample<float4>(sampler,uv);
            float3 normal = d.texture2.sample<float4>(sampler,uv).rgb*2-1;
            float boundary = 0;
            const float2 neighbours[8] = {
                float2(1,0),float2(-1,0),float2(0,1),float2(0,-1),
                float2(1,1),float2(-1,1),float2(1,-1),float2(-1,-1)
            };
            for (int i=0;i<8;++i) {
                float2 q = uv+neighbours[i]*texel;
                float alpha = d.texture1.sample<float4>(sampler,q).a;
                boundary = max(boundary,abs(alpha-surface.a));
                if (alpha > 0.999 && surface.a > 0.999)
                    boundary = max(boundary,smoothstep(0.85,1.35,
                        length(d.texture2.sample<float4>(sampler,q).rgb*2-1-normal)));
            }
            if (boundary < 0.01) return float4(center,1);
        }
        float3 nw = resolvedColor(d,sampler,uv+float2(-1,-1)*texel);
        float3 ne = resolvedColor(d,sampler,uv+float2(1,-1)*texel);
        float3 sw = resolvedColor(d,sampler,uv+float2(-1,1)*texel);
        float3 se = resolvedColor(d,sampler,uv+float2(1,1)*texel);
        float3 weights = float3(0.299,0.587,0.114);
        float lc=dot(center,weights), lnw=dot(nw,weights), lne=dot(ne,weights), lsw=dot(sw,weights), lse=dot(se,weights);
        float lo=min(lc,min(min(lnw,lne),min(lsw,lse))), hi=max(lc,max(max(lnw,lne),max(lsw,lse)));
        if (hi-lo < max(0.025,hi*0.12)) return float4(center,1);
        float2 dir=float2(-((lnw+lne)-(lsw+lse)),(lnw+lsw)-(lne+lse));
        float reduce=max((lnw+lne+lsw+lse)*0.03125,0.0078125);
        float span = d.scale > 1.5 ? 4 : 8;
        dir=clamp(dir/(min(abs(dir.x),abs(dir.y))+reduce),-span,span)*texel;
        float3 a=0.5*(resolvedColor(d,sampler,uv+dir*(-1.0/6))+resolvedColor(d,sampler,uv+dir*(1.0/6)));
        float3 b=a*0.5+0.25*(resolvedColor(d,sampler,uv-dir*0.5)+resolvedColor(d,sampler,uv+dir*0.5));
        float lb=dot(b,weights);
        return float4(lb<lo||lb>hi?a:b,1);
    }
    if (d.settings.x < 3.5) {
        float3 ray = normalize(unproject(f, uv, 0.99) - f.eye.xyz);
        float3 color = sky(ray);
        float t = -f.eye.y / ray.y;
        if (ray.y < -0.0001 && t > 0) {
            float3 p = f.eye.xyz + ray*t;
            float distance = length(p.xz);
            float grassVariation = 0.5+0.5*sin(p.x*0.8)*cos(p.z*0.9)*exp(-distance*0.10);
            float3 grass = lerp(float3(0.43,0.62,0.41), float3(0.58,0.72,0.48), grassVariation);
            float radius = length(p.xz);
            if (radius < 2.2) {
                float ring = smoothstep(0.018,0.025, abs(radius-2.04));
                float tile = step(0.025, min(abs(frac(p.x*1.3+0.5)-0.5), abs(frac(p.z*1.3+0.5)-0.5)));
                grass = lerp(float3(0.56,0.65,0.65), float3(0.81,0.84,0.78), tile) * (0.88+0.12*ring);
            }
            float visibility = shadowAt(f, d.texture0, p, float3(0,1,0));
            grass *= lerp(float3(0.60,0.66,0.78), float3(1.02,1.01,0.97), visibility);
            // Small contact term anchors the character at the center of the world.
            grass *= 1 - 0.16*exp(-dot(p.xz,p.xz)*6);
            color = lerp(grass, float3(0.78,0.87,0.91), 1-exp(-distance*0.022));
        }
        return float4(color,1);
    }
    float4 toon = d.texture0.load<float4>(pixel);
    float3 world = d.texture1.load<float4>(pixel).rgb;
    float3 color = toon.a > 0.5 ? toon.rgb : world;
    float edge = 0;
    // The contour straddles the silhouette, so each side uses half the width.
    float radius = d.settings.w*0.5;
    float2 texel = 1.0/d.settings.yz;
    SamplerState sampler = d.sampler.load();
    float3 centerNormal = d.texture3.load<float4>(pixel).rgb*2-1;
    // A circular footprint avoids the axis bias of four integer neighbour taps.
    // Linear alpha sampling provides fractional coverage and continuous width.
    const float2 directions[8] = {
        float2(1,0),float2(-1,0),float2(0,1),float2(0,-1),
        float2(.70710678,.70710678),float2(-.70710678,.70710678),
        float2(.70710678,-.70710678),float2(-.70710678,-.70710678)
    };
    for (int i=0;i<8;++i) {
        float2 q = uv+directions[i]*radius*texel;
        float4 other = d.texture0.sample<float4>(sampler,q);
        edge = max(edge, abs(other.a-toon.a));
        if (toon.a > 0.5 && other.a > 0.999) {
            float3 normal = d.texture3.sample<float4>(sampler,q).rgb*2-1;
            edge = max(edge, smoothstep(0.85,1.35,length(normal-centerNormal))*0.22);
        }
    }
    if (d.settings.w > 0) color = lerp(color, color*0.3+float3(0.035,0.025,0.04),edge*0.78);
    return float4(color,1);
}
