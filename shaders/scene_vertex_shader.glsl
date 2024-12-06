#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform vec2 uResolution;
uniform vec2 uPosition;
uniform vec2 uSize;

out vec2 TexCoord;

void main() {
    vec2 normalizedPos = (aPos.xy * uSize + uPosition) / uResolution * 2.0 - 1.0;
    gl_Position = vec4(normalizedPos, aPos.z, 1.0);
    TexCoord = aTexCoord;
}