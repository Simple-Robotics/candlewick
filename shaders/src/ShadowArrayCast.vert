#version 450

#include "config.glsl"

layout(location = 0) in vec3 inPosition;

layout(set = 1, binding = 0) uniform LightBlock {
    mat4 viewProjs[NUM_LIGHTS];
    int numLights;
};

layout(set = 1, binding = 1) uniform Model {
    mat4 model;
};

out gl_PerVertex {
    invariant vec4 gl_Position;
};

void main() {
    int lightIndex = gl_InstanceIndex;
    vec4 worldPos = model * vec4(inPosition, 1.0);
    gl_Position = viewProjs[lightIndex] * worldPos;
}
