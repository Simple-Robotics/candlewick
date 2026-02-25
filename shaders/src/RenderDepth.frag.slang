// To be used with DrawQuad.vert
import utils;

#define VIZ_GRAYSCALE   0
#define VIZ_HEATMAP     1

[vk::binding(0, 2)] Sampler2D depthTex;

struct CameraParams {
    int mode;
    float near;
    float far;
    uint isOrtho; // 0 for perspective, 1 for ortho
};

[vk::binding(0, 3)] ConstantBuffer<CameraParams> camera;

float3 visualizeDepthGrayscale(float depth) {
    float lin;
    if (camera.isOrtho == 1) {
        lin = linearizeDepthOrtho(depth, camera.near, camera.far);
    } else {
        lin = linearizeDepth(depth, camera.near, camera.far);
    }
    float norm = (lin - camera.near) / (camera.far - camera.near);
    return float3(norm);
}

float3 visualizeDepthHeatmap(float depth) {
    float lin;
    if (camera.isOrtho == 1) {
        lin = linearizeDepthOrtho(depth, camera.near, camera.far);
    } else {
        lin = linearizeDepth(depth, camera.near, camera.far);
    }
    float normalized = (lin - camera.near) / (camera.far - camera.near);

    float3 color;
    if (normalized < 0.25) {
        float t = normalized / 0.25;
        color = lerp(float3(0, 0, 1), float3(0, 1, 1), t);
    } else if (normalized < 0.5) {
        float t = (normalized - 0.25) / 0.25;
        color = lerp(float3(0, 1, 1), float3(0, 1, 0), t);
    } else if (normalized < 0.75) {
        float t = (normalized - 0.5) / 0.25;
        color = lerp(float3(0, 1, 0), float3(1, 1, 0), t);
    } else {
        float t = (normalized - 0.75) / 0.25;
        color = lerp(float3(1, 1, 0), float3(1, 0, 0), t);
    }
    return color;
}

[shader("fragment")]
float4 main([vk::location(0)] float2 inUV) : SV_Target0 {
    float depth = depthTex.Sample(inUV).r;

    float3 color;
    switch (camera.mode) {
        case VIZ_GRAYSCALE:
            color = visualizeDepthGrayscale(depth);
            break;
        case VIZ_HEATMAP:
            color = visualizeDepthHeatmap(depth);
            break;
        default:
            color = float3(0);
            break;
    }
    return float4(color, 1.0);
}
