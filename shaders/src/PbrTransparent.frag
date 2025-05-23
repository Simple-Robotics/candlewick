#version 450
#define HAS_WBOIT
#define HAS_SHADOW_MAPS
#define HAS_SSAO

#include "tone_mapping.glsl"
#include "pbr_lighting.glsl"
#include "config.glsl"

layout(location=0) in vec3 fragViewPos;
layout(location=1) in vec3 fragViewNormal;
layout(location=2) in vec3 fragLightPos;

// layout declarations with two outputs
layout(location = 0) out vec4 accum; // R16G16B16A16 accumulation
layout(location = 1) out float reveal; // R8 revealage

layout (set=3, binding=0) uniform Material {
    // material diffuse color
    PbrMaterial material;
};

layout(set=3, binding=1) uniform LightBlock {
    vec3 direction[NUM_LIGHTS];
    vec3 color[NUM_LIGHTS];
    float intensity[NUM_LIGHTS];
    int numLights;
} light;

layout(set=3, binding=2) uniform EffectParams {
    uint useSsao;
} params;

#ifdef HAS_SHADOW_MAPS
    layout (set=2, binding=0) uniform sampler2DShadow shadowMap;
#endif
#ifdef HAS_SSAO
    layout (set=2, binding=1) uniform sampler2D ssaoTex;
#endif


void main() {
    vec3 normal = normalize(fragViewNormal);
    vec3 V = normalize(-fragViewPos);

    vec3 Lo = vec3(0.);
    for (uint i = 0; i < light.numLights; i++) {
        vec3 lightDir = normalize(-light.direction[i]);
        Lo += calculatePbrLighting(
            normal,
            V,
            lightDir,
            material,
            light.color[i],
            light.intensity[i]
        );
    }

    vec3 ambient = vec3(0.03) * material.baseColor.rgb * material.ao;
    vec3 color = ambient + Lo;

    // Tone mapping and gamma correction
    color = uncharted2ToneMapping(color);
    color = pow(color, vec3(1.0/2.2));

    // WBOIT specific calculations
    float alpha = material.baseColor.a;

    const float z = gl_FragCoord.z;
    float w1 = alpha * 10. + 0.01;
    float weight = clamp(w1 * (1.0 - z * 0.3), 1e-2, 1e3);

    // Outputs for WBOIT
    accum = vec4(color * alpha * weight, alpha);
    reveal = 1. - alpha * weight;
}
