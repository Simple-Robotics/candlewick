#version 450
#define HAS_SHADOW_MAPS
#define HAS_G_BUFFER
#define HAS_SSAO

#include "utils.glsl"
#include "tone_mapping.glsl"
#include "pbr_lighting.glsl"

layout(location=0) in vec3 fragViewPos;
layout(location=1) in vec3 fragViewNormal;
layout(location=2) in vec3 fragLightPos;

// set=3 is required, see SDL3's documentation for SDL_CreateGPUShader
// https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader
layout (set=3, binding=0) uniform Material {
    // material diffuse color
    PbrMaterial material;
};

layout(set=3, binding=1) uniform LightBlock {
    vec3 direction;
    vec3 color;
    float intensity;
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

layout(location=0) out vec4 fragColor;
#ifdef HAS_G_BUFFER
    // output normals for post-effects
    layout(location=1) out vec2 outNormal;
#endif

#ifdef HAS_SHADOW_MAPS
float calcShadowmap(float NdotL) {
    // float bias = max(0.05 * (1.0 - NdotL), 0.005);
    float bias = 0.005;
    vec3 texCoords = fragLightPos;
    texCoords.x = 0.5 + texCoords.x * 0.5;
    texCoords.y = 0.5 - texCoords.y * 0.5;
    texCoords.z -= bias;
    float shadowValue = 1.0;
    if (isCoordsInRange(texCoords)) {
        shadowValue = texture(shadowMap, texCoords);
    }
    return shadowValue;
}
#endif

void main() {
    vec3 lightDir = normalize(-light.direction);
    vec3 normal = normalize(fragViewNormal);
    vec3 V = normalize(-fragViewPos);

    if (!gl_FrontFacing) {
        // Flip normal for back faces
        normal = -normal;
    }

    vec3 Lo = calculatePbrLighting(
        normal,
        V,
        lightDir,
        material,
        light.color,
        light.intensity
    );

#ifdef HAS_SHADOW_MAPS
        float NdotL = max(dot(normal, lightDir), 0.0);
        float shadowValue = calcShadowmap(NdotL);
        Lo = shadowValue * Lo;
#endif

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
