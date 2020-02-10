#version 330

layout (location = 0) in vec3 POSITION;
layout (location = 1) in vec3 NORMAL;
layout (location = 2) in vec2 TEXCOORD;

out vec4 normal;
out vec4 v;
out vec2 uv;

uniform mat4 modelMatrix;

uniform mat4 worldMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main(){

    mat4 transformMatrix = worldMatrix * modelMatrix;

    v = transformMatrix * vec4(POSITION, 1.0f);
    v = viewMatrix * v;
    gl_Position = projectionMatrix * v;
    
    normal = transformMatrix * vec4(NORMAL, 0.0f);
    normal = viewMatrix * normal;

    uv = TEXCOORD;
}