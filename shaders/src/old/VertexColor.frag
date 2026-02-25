#version 450

layout(location=0) in vec4 interpColor;

layout(location=0) out vec4 FragColor;

void main() {

    FragColor = interpColor;

}
