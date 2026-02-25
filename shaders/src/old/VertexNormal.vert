#version 450

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;

layout(location=0) out vec3 f_normal;

// set=1 is required, see the documentation on SDL3's shader layout
// https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader
layout(set=1, binding=0) uniform TranformBlock
{
    mat4 mvp;
    mat3 normalMatrix;
};

void main() {

    f_normal = normalize(normalMatrix * normal);
    gl_Position = mvp * vec4(position, 1.0);

}
