#version 330 core

uniform vec3 lightPosition;
uniform vec4 lightColor;

uniform mat4 worldMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

in vec4 normal;
in vec4 v;

out vec4 outputColor;

void main(void) {

    vec3 N = normalize(normal.xyz);
    vec3 L = normalize((viewMatrix * vec4(lightPosition, 1.0f)).xyz - v.xyz);
    vec3 R = normalize(2 * dot(L,N) * N - L);
    vec3 V = normalize(v.xyz);
    float diffuse = dot(N, L);

    outputColor = vec4(lightColor.xyz * clamp(diffuse + 0.01 * dot(R, V), 0.0f, 1.0f), 1.0f);
}