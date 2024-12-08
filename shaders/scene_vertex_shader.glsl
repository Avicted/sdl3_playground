#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 u_mvp;

out vec2 TexCoord;

void main()
{
    gl_Position = u_mvp * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
