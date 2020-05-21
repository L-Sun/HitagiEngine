#version 330 core

uniform vec3 lineColor;

out vec4 outputColor;

void main()
{
    outputColor = vec4(lineColor, 1.0f);
    }