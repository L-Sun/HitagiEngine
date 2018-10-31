#version 330

in vec3 inputPosition;
in vec3 inputNormal;

out vec4 normal;
out vec4 v;

uniform mat4 objectLocalMatrix;

uniform mat4 worldMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main(){

    mat4 transformMatrix = worldMatrix * objectLocalMatrix;

    v = transformMatrix * vec4(inputPosition, 1.0f);
    v = viewMatrix * v;
    gl_Position = projectionMatrix * v;
    
    normal = transformMatrix * vec4(inputNormal, 0.0f);
    normal = viewMatrix * normal;
}