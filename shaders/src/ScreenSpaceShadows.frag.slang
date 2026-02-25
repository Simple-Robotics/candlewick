import utils;

[vk::binding(0, 2)] Sampler2D depthTexture;

struct ShadowParams {
    float4x4 projection;
    float4x4 invProjection;
    float3 lightDir;
    float maxDistance;
    int numSteps;
};

[vk::binding(0, 3)] ConstantBuffer<ShadowParams> shadow;

static const float SSS_THICKNESS = 0.02;
static const float SSS_MAX_RAY_DISTANCE = 0.05;

float3 computeViewPos(float3 ndcPos) {
    float4 viewPos = mul(shadow.invProjection, float4(ndcPos, 1.0));
    return viewPos.xyz / viewPos.w;
}

[shader("fragment")]
float main([vk::location(0)] float2 fragUV) : SV_Target0 {
    float depth = depthTexture.Sample(fragUV).r;

    if (depth >= 1.0) {
        return 1.0;
    }

    const float3 ndcPos = float3(fragUV * 2.0 - 1.0, depth);
    const float3 viewPos = computeViewPos(ndcPos);

    float3 rayPos = viewPos;

    const float stepSize = SSS_MAX_RAY_DISTANCE / float(shadow.numSteps);
    float3 toLight = normalize(-shadow.lightDir);
    float3 rayStep = toLight * stepSize;

    float depthDelta;
    float occlusion = 0.0;

    for (int i = 0; i < shadow.numSteps; i++) {
        rayPos += rayStep;

        float4 projectedPos = mul(shadow.projection, float4(rayPos, 1.0));
        float3 screenPos = projectedPos.xyz / projectedPos.w;

        float2 rayUV = 0.5 + screenPos.xy * 0.5;
        float rayDepth = screenPos.z;
        float sceneDepth = depthTexture.Sample(rayUV).r;

        if (!isCoordsInRange(rayUV)) {
            break;
        }

        depthDelta = rayDepth - sceneDepth - 0.001;

        if ((depthDelta >= 0.0) && (depthDelta < SSS_THICKNESS)) {
            occlusion = 1.0;
            break;
        }
    }

    return 1.0 - occlusion;
}
