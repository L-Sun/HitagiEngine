#version 330

in vec3 inputPosition;
uniform mat4 worldMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main(){

    mat4 transformMatrix = projectionMatrix * viewMatrix * worldMatrix;
    gl_Position= transformMatrix * vec4(inputPosition, 1.0f);
    
}