#version 450
#define HAS_SHADOW_MAPS
#define HAS_G_BUFFER
#define HAS_SSAO

#include "utils.glsl"
#include "tone_mapping.glsl"
#include "pbr_lighting.glsl"
#include "config.glsl"

layout(location=0) in vec3 fragViewPos;
layout(location=1) in vec3 fragViewNormal;
layout(location=2) in vec3 fragLightPos[NUM_LIGHTS];

// set=3 is required, see SDL3's documentation for SDL_CreateGPUShader
// https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader
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

layout(set = 3, binding = 3) uniform ShadowAtlasInfo {
    ivec4 lightRegions[NUM_LIGHTS];
};

#ifdef HAS_SHADOW_MAPS
    layout (set=2, binding=0) uniform sampler2DShadow shadowMap;
#endif
#ifdef HAS_SSAO
    layout (set=2, binding=1) uniform sampler2D ssaoTex;
#endif

layout(location=0) out vec4 fragColor;
#ifdef HAS_G_BUFFER
    // output normals for post-effects
    layout(location=1) out vec2 outNormal;
#endif

#ifdef HAS_SHADOW_MAPS
float calcShadowmap(int lightIndex, float NdotL, ivec2 atlasSize) {
    // float bias = max(0.05 * (1.0 - NdotL), 0.005);
    float bias = 0.005;
    vec3 lightSpacePos = fragLightPos[lightIndex];
    vec2 uv;
    uv.x = 0.5 + lightSpacePos.x * 0.5;
    uv.y = 0.5 - lightSpacePos.y * 0.5;
    float depthRef = lightSpacePos.z - bias;
    if (!isCoordsInRange(vec3(uv, depthRef))) {
        return 1.0;
    }

    ivec4 region = lightRegions[lightIndex];
    uv = region.xy + uv * region.zw;
    uv = uv / atlasSize;

    vec3 texCoords = vec3(uv, depthRef);
    return texture(shadowMap, texCoords);
}
#endif

void main() {
    vec3 normal = normalize(fragViewNormal);
    const vec3 V = normalize(-fragViewPos);
#ifdef HAS_SHADOW_MAPS
    const ivec2 atlasSize = textureSize(shadowMap, 0).xy;
#endif

    if (!gl_FrontFacing) {
        // Flip normal for back faces
        normal = -normal;
    }

    vec3 Lo = vec3(0);
    for(int i = 0; i < light.numLights; i++) {
        vec3 lightDir = normalize(-light.direction[i]);
        vec3 _lo = calculatePbrLighting(
            normal,
            V,
            lightDir,
            material,
            light.color[i],
            light.intensity[i]
        );
#ifdef HAS_SHADOW_MAPS
        const float NdotL = max(dot(normal, lightDir), 0.0);
        const float shadowValue = calcShadowmap(i, NdotL, atlasSize);
        _lo *= shadowValue;
#endif
        Lo += _lo;
    }

    // Ambient term (very simple)
    vec3 ambient = vec3(0.1) * material.baseColor.rgb * material.ao;

#ifdef HAS_SSAO
        if(params.useSsao == 1) {
            vec2 ssaoTexSize = textureSize(ssaoTex, 0).xy;
            vec2 ssaoUV = gl_FragCoord.xy / ssaoTexSize;
            ambient *= texture(ssaoTex, ssaoUV).r;
        }
#endif

    // Final color
    vec3 color = ambient + Lo;

    // Tone mapping and gamma correction
    color = uncharted2ToneMapping(color);
    color = pow(color, vec3(1.0/2.2));

    // Output
    fragColor = vec4(color, material.baseColor.a);
#ifdef HAS_G_BUFFER
        outNormal = fragViewNormal.rg;
#endif
}
