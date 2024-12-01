#version 450

layout(location=0) in vec3 inPosition;
layout(location=2) in vec4 inColor;

layout(set=1, binding=0) uniform TranformBlock {
    mat4 mvp;
};

layout(location=0) out vec4 outColor;

void main() {
    gl_Position = mvp * vec4(inPosition, 1.0);
    outColor = inColor;
}
