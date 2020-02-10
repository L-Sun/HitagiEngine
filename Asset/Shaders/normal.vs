#version 330 core
layout (location = 0) in vec3 inputPosition;
layout (location = 1) in vec3 inputNormal;

out VS_OUT {
    vec3 normal;
} vs_out;

uniform mat4 worldMatrix;
uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;

void main()
{
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(inputPosition, 1.0); 
    mat3 normalMatrix = mat3(transpose(inverse(viewMatrix * modelMatrix)));
    vs_out.normal = normalize(vec3(projectionMatrix * vec4(normalMatrix * inputNormal, 0.0)));
}