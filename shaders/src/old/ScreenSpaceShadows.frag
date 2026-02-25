#version 450

#include "utils.glsl"

layout(location = 0) in vec2 fragUV;
layout(location = 0) out float fragShadow;

// Depth texture from pre-pass
layout(set=2, binding=0) uniform sampler2D depthTexture;

// Parameters for the effect
layout(set=3, binding=0) uniform ShadowParams {
    // Camera projection matrix
    mat4 projection;
    // Inverse projection matrix
    mat4 invProjection;
    // View-space light direction
    vec3 lightDir;
    // Maximum ray march distance
    float maxDistance;
    // Maximum number of steps
    int numSteps;
};

// reconstruct view-space position from depth
vec3 computeViewPos(vec3 ndcPos) {
    vec4 viewPos = invProjection * vec4(ndcPos, 1.0);
    return viewPos.xyz / viewPos.w;
}

const float SSS_THICKNESS = 0.02;
const float SSS_MAX_RAY_DISTANCE = 0.05;

void main() {
    // Sample depth
    float depth = texture(depthTexture, fragUV).r;

    if (depth >= 1.0) {
        fragShadow = 1.0;
        return;
    }

    // reconstruct view-space position
    const vec3 ndcPos = vec3(fragUV * 2. - 1., depth);
    const vec3 viewPos = computeViewPos(ndcPos);

    // initial ray position
    vec3 rayPos = viewPos;

    const float stepSize = SSS_MAX_RAY_DISTANCE / float(numSteps);
    vec3 toLight = normalize(-lightDir);
    vec3 rayStep = toLight * stepSize;

    float depthDelta;
    float occlusion = 0.0;

    // march a ray from the fragment towards the light
    for (int i = 0; i < numSteps; i++) {
        // advance ray
        rayPos += rayStep;

        // project current ray position to screen space
        vec4 projectedPos = projection * vec4(rayPos, 1.0);
        vec3 screenPos = projectedPos.xyz / projectedPos.w;

        // convert to UV coordinates
        vec2 rayUV = 0.5 + screenPos.xy * 0.5;

        // depth at the ray depth
        float rayDepth = screenPos.z;
        float sceneDepth = texture(depthTexture, rayUV).r;


        if(!isCoordsInRange(rayUV)) {
            break;
        }

        depthDelta = rayDepth - sceneDepth - 0.001;

        // compare depths: if the current depth is closer than the ray,
        // then fragment is occluded.
        if ((depthDelta >= 0.0) && (depthDelta < SSS_THICKNESS)) {
            occlusion = 1.0;
            break;
        }
    }

    // No shadow found
    fragShadow = 1.0 - occlusion;
}
