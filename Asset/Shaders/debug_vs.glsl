#version 330

in vec3 POSITION;
uniform mat4 worldMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main(){

    mat4 transformMatrix = projectionMatrix * viewMatrix * worldMatrix;
    gl_Position= transformMatrix * vec4(POSITION, 1.0f);
    
}