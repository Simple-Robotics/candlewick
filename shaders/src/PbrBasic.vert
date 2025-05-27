#version 450

#include "config.glsl"

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;

// world-space position-normal
layout(location=0) out vec3 fragViewPos;
layout(location=1) out vec3 fragViewNormal;
layout(location=2) out vec3 fragLightPos[MAX_NUM_LIGHTS];


// set=1 is required, for some reason
layout(set=1, binding=0) uniform TranformBlock
{
    mat4 modelView;
    mat4 mvp;
    mat3 normalMatrix;
};

layout(set=1, binding=1) uniform LightBlockV
{
    mat4 mvp[MAX_NUM_LIGHTS];
    int numLights;
} lights;

out gl_PerVertex {
    invariant vec4 gl_Position;
};

void main() {
    vec4 hp = vec4(inPosition, 1.0);
    fragViewPos = vec3(modelView * hp);
    fragViewNormal = normalize(normalMatrix * inNormal);
    gl_Position = mvp * hp;

    for (uint i = 0; i < lights.numLights; i++) {
        vec4 flps = lights.mvp[i] * hp;
        fragLightPos[i] = flps.xyz / flps.w;
    }
}
