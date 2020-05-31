#version 330 core

uniform vec4 lightPosInView;
uniform vec4 lightColor;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

uniform vec3 ambientColor;
uniform vec3 diffuseColor;
uniform vec3 specularColor;
uniform float specularPower;

uniform sampler2D defaultSampler;

in vec4 normal;
in vec4 v;
in vec2 uv;

out vec4 outputColor;

void main(void) {

    vec3 N = normalize(normal.xyz);
    vec3 L = normalize(lightPosInView.xyz - v.xyz);
    vec3 H = normalize(L.xyz - normalize(v.xyz));
    float r = length(lightPosInView.xyz - v.xyz);
    float inv2 = 1.0f / (r* r + 1.0f);
    if (diffuseColor.r < 0)
        outputColor = vec4(ambientColor.rgb + lightColor.rgb * inv2 * (texture(defaultSampler, uv).rgb * max(dot(N, L), 0.0f) + specularColor.rgb * pow(max(dot(H, N), 0.0f), specularPower)), 1.0f);  
    else
        outputColor = vec4(ambientColor.rgb + lightColor.rgb * inv2 * (diffuseColor.rgb * max(dot(N, L), 0.0f) + specularColor.rgb * pow(max(dot(H, N), 0.0f), specularPower)), 1.0f);  
}