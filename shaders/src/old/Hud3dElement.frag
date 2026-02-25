#version 450

layout(location=0) out vec4 fragColor;

layout(set=3, binding=0) uniform Color {
    vec4 color;
};

void main() {
    fragColor = color;
}
