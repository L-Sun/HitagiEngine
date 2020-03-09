#version 330 core

layout (location = 0) in vec3 POSITION;

uniform mat4 WVP;


void main(){
    gl_Position= WVP * vec4(POSITION, 1.0f);
}