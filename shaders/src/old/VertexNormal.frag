#version 450

layout(location=0) in vec3 f_normal;

layout(location=0) out vec4 FragColor;

void main() {

    FragColor.xyz = f_normal;

}
