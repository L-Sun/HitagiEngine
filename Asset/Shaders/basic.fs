#version 330 core

uniform vec3 lightPosition;
uniform vec4 lightColor;

uniform mat4 worldMatrix;
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
    vec3 L = normalize((viewMatrix * vec4(lightPosition, 1.0f)).xyz - v.xyz);
    vec3 R = normalize(2 * clamp(dot(L,N), 0.0f, 1.0f) * N - L);
    vec3 V = normalize(v.xyz);
    if (diffuseColor.r < 0)
        outputColor = vec4(ambientColor.rgb + lightColor.rgb* texture(defaultSampler, uv).rgb * clamp(dot(N, L),0.0f, 1.0f) + specularColor.rgb * pow(clamp(dot(R, V), 0.0f, 1.0f), specularPower), 1.0f); 
    else
        outputColor = vec4(ambientColor.rgb + lightColor.rgb * diffuseColor.rgb * clamp(dot(N, L),0.0f, 1.0f) + specularColor.rgb * pow(clamp(dot(R,V), 0.0f, 1.0f), specularPower), 1.0f);  
}