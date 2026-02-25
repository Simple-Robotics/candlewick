#version 450

layout(location=0) in vec3 inPosition;

layout(set=1, binding=0) uniform TranformBlock {
    mat4 mvp;
};

void main() {
    gl_Position = mvp * vec4(inPosition, 1.0);
}
