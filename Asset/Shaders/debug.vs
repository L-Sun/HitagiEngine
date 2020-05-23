#version 330 core

layout (location = 0) in vec3 POSITION;

uniform mat4 projView;

void main(){
    gl_Position= projView * vec4(POSITION, 1.0f);
}