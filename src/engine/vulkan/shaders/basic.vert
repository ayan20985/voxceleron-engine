#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inColor;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    fragColor = vec4(
        float((inColor >> 16) & 0xFF) / 255.0,
        float((inColor >> 8) & 0xFF) / 255.0,
        float(inColor & 0xFF) / 255.0,
        float((inColor >> 24) & 0xFF) / 255.0
    );
} 