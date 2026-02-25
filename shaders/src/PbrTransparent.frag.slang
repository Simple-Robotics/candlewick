#define HAS_WBOIT
#define HAS_SHADOW_MAPS
#define HAS_SSAO

import tone_mapping;
import pbr_lighting;
import config;

struct MaterialBlock {
    PbrMaterial material;
};

struct LightBlock {
    float3 direction[MAX_NUM_LIGHTS];
    float3 color[MAX_NUM_LIGHTS];
    float intensity[MAX_NUM_LIGHTS];
    int numLights;
};

struct EffectParams {
    uint useSsao;
};

[vk::binding(0, 3)] ConstantBuffer<MaterialBlock> materialBlock;
[vk::binding(1, 3)] ConstantBuffer<LightBlock>    light;
[vk::binding(2, 3)] ConstantBuffer<EffectParams>  params;

#ifdef HAS_SHADOW_MAPS
    [vk::binding(0, 2)] Sampler2DShadow shadowMap;
#endif
#ifdef HAS_SSAO
    [vk::binding(1, 2)] Sampler2D ssaoTex;
#endif

struct FSOutput {
    float4 accum  : SV_Target0; // R16G16B16A16 accumulation
    float  reveal : SV_Target1; // R8 revealage
};

[shader("fragment")]
FSOutput main([vk::location(0)] float3 fragViewPos,
              [vk::location(1)] float3 fragViewNormal,
              [vk::location(2)] float3 fragLightPos,
              float4 fragCoord : SV_Position) {
    float3 normal = normalize(fragViewNormal);
    float3 V = normalize(-fragViewPos);

    PbrMaterial mat = materialBlock.material;

    float3 Lo = float3(0);
    for (uint i = 0; i < uint(light.numLights); i++) {
        float3 lightDir = normalize(-light.direction[i]);
        Lo += calculatePbrLighting(normal, V, lightDir, mat,
                                   light.color[i], light.intensity[i]);
    }

    float3 ambient = float3(0.03) * mat.baseColor.rgb * mat.ao;
    float3 color = ambient + Lo;

    color = uncharted2ToneMapping(color);
    color = pow(color, float3(1.0 / 2.2));

    float alpha = mat.baseColor.a;
    const float z = fragCoord.z;
    float w1 = alpha * 10.0 + 0.01;
    float weight = clamp(w1 * (1.0 - z * 0.3), 1e-2, 1e3);

    FSOutput output;
    output.accum  = float4(color * alpha * weight, alpha);
    output.reveal = 1.0 - alpha * weight;
    return output;
}
