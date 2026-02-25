[vk::binding(0, 2)] Sampler2D depthTex;
[vk::binding(1, 2)] Sampler2D normalMap;
[vk::binding(2, 2)] Sampler2D ssaoNoise;

static const int SSAO_MAX_KERNEL_SIZE = 64;
static const float SSAO_RADIUS = 1.0;
static const float SSAO_BIAS = 0.01;
static const float SSAO_INTENSITY = 1.5;

struct SSAOParams {
    float4 samples[SSAO_MAX_KERNEL_SIZE];
};

struct Camera {
    float4x4 projection;
    float4x4 projectionInverse;
    uint kernelSize;
};

[vk::binding(0, 3)] ConstantBuffer<SSAOParams> kernel;
[vk::binding(1, 3)] ConstantBuffer<Camera> camera;

float3 getViewPos(float depth, float2 uv) {
    float4 clipPos = float4(uv * 2.0 - 1.0, depth, 1.0);
    float4 viewPos = mul(camera.projectionInverse, clipPos);
    return viewPos.xyz / viewPos.w;
}

float3 sampleNoiseTexture(float2 uv) {
    uint dW, dH;
    depthTex.GetDimensions(dW, dH);
    float2 viewport = float2(dW, dH);
    float2 noiseScale = viewport / 4.0;
    uv = uv * noiseScale;
    return float3(ssaoNoise.Sample(uv).rg, 0);
}

float calculatePixelAO(float2 uv) {
    float depth = depthTex.Sample(uv).r;
    float3 viewPos = getViewPos(depth, uv);
    float3 viewNormal;
    viewNormal.xy = normalMap.Sample(uv).xy;
    viewNormal.z = sqrt(1 - dot(viewNormal.xy, viewNormal.xy));

    float3 randVec = sampleNoiseTexture(uv);

    // TBN matrix for rotating samples from tangent space to view space
    float3 tangent   = normalize(randVec - viewNormal * dot(randVec, viewNormal));
    float3 bitangent = cross(tangent, viewNormal);
    // mul(v, M) where M has rows [tangent, bitangent, viewNormal] is equivalent
    // to GLSL's mat3(tangent, bitangent, viewNormal) * v (column-major multiply)

    float occlusion = 0.0;
    for (int i = 0; i < int(camera.kernelSize); i++) {
        float3 samplePos = mul(kernel.samples[i].xyz, float3x3(tangent, bitangent, viewNormal));
        samplePos = viewPos + samplePos * SSAO_RADIUS;

        float4 offset = mul(camera.projection, float4(samplePos, 1.0));
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;

        float sampleDepth = depthTex.Sample(offset.xy).r;
        float3 sampleViewPos = getViewPos(sampleDepth, offset.xy);

        float rangeCheck = smoothstep(0.0, 1.0, SSAO_RADIUS / abs(viewPos.z - sampleViewPos.z - SSAO_BIAS));
        occlusion += (sampleViewPos.z >= samplePos.z + SSAO_BIAS ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(camera.kernelSize)) * SSAO_INTENSITY;
    return occlusion;
}

[shader("fragment")]
float main([vk::location(0)] float2 inUV) : SV_Target0 {
    return calculatePixelAO(inUV);
}
