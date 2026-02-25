#version 450


layout(location=0) in vec2 inUV;
layout(location=0) out float aoValue;

layout(set=2, binding=0) uniform sampler2D depthTex;
layout(set=2, binding=1) uniform sampler2D normalMap;
layout(set=2, binding=2) uniform sampler2D ssaoNoise;

// Maximum number of kernel samples; actual count is camera.kernelSize.
const int SSAO_MAX_KERNEL_SIZE = 64;
const float SSAO_RADIUS = 1.0;
const float SSAO_BIAS = 0.01;
const float SSAO_INTENSITY = 1.5;

layout(set=3, binding=0) uniform SSAOParams {
    vec4 samples[SSAO_MAX_KERNEL_SIZE];
} kernel;

layout(set=3, binding=1) uniform Camera {
    mat4 projection;
    mat4 projectionInverse;
    uint kernelSize;
} camera;

vec3 getViewPos(float depth, vec2 uv) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = camera.projectionInverse * clipPos;
    return viewPos.xyz / viewPos.w;
}

vec3 sampleNoiseTexture(vec2 uv) {
    vec2 viewport = textureSize(depthTex, 0).xy;
    vec2 noiseScale = viewport / 4.0;
    uv = uv * noiseScale;
    return vec3(texture(ssaoNoise, uv).rg, 0);
}

float calculatePixelAO(vec2 uv) {
    float depth = texture(depthTex, uv).r;
    vec3 viewPos = getViewPos(depth, uv);
    vec3 viewNormal;
    viewNormal.xy = texture(normalMap, uv).xy;
    viewNormal.z = sqrt(1 - dot(viewNormal.xy, viewNormal.xy));

    vec3 randVec = sampleNoiseTexture(uv);

    // tbn matrix for rotating samples
    vec3 tangent = normalize(randVec - viewNormal * dot(randVec, viewNormal));
    vec3 bitangent = cross(tangent, viewNormal);
    mat3 TBN = mat3(tangent, bitangent, viewNormal);

    // accumulate occlusion
    float occlusion = 0.0;
    for(int i = 0; i < int(camera.kernelSize); i++) {
        // get sample position
        vec3 samplePos = TBN * kernel.samples[i].xyz; // Rotate sample vector
        samplePos = viewPos + samplePos * SSAO_RADIUS;         // Move it to view-space position

        // project sample to get its screen-space coordinates
        vec4 offset = camera.projection * vec4(samplePos, 1.0);
        offset.xy /= offset.w;                            // Perspective divide
        offset.xy = offset.xy * 0.5 + 0.5;               // Transform to [0,1] range

        // get sample depth
        float sampleDepth = texture(depthTex, offset.xy).r;

        // convert sample's depth to view space for comparison
        vec3 sampleViewPos = getViewPos(sampleDepth, offset.xy);

        // Rarnge check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, SSAO_RADIUS / abs(viewPos.z - sampleViewPos.z - SSAO_BIAS));
        occlusion += (sampleViewPos.z >= samplePos.z + SSAO_BIAS ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(camera.kernelSize)) * SSAO_INTENSITY;
    return occlusion;
}

void main() {
    aoValue = calculatePixelAO(inUV);
}
