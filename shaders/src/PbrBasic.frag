#version 450
#define HAS_SHADOW_MAPS
#define HAS_G_BUFFER
#define HAS_SSAO

#include "tone_mapping.glsl"
#include "pbr_material.glsl"

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
    // direction in NDC space
    mat4 camProjection;
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

// Constants
const float PI = 3.14159265359;
const float F0 = 0.04; // Standard base reflectivity

bool isCoordsInRange(vec3 uv) {
    return uv.x >= 0.0 &&
           uv.y >= 0.0 &&
           uv.x <= 1.0 &&
           uv.y <= 1.0 &&
           uv.z >= 0.0 &&
           uv.z <= 1.0;
}

bool isCoordsInRange(vec2 uv) {
    return uv.x >= 0.0 &&
           uv.y >= 0.0 &&
           uv.x <= 1.0 &&
           uv.y <= 1.0;
}

// Schlick's Fresnel approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float distributionGGX(vec3 normal, vec3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(normal, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / denom;
}

// Geometry function (Smith's method with Schlick-GGX)
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometrySmith(vec3 normal, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(normal, V), 0.0);
    float NdotL = max(dot(normal, L), 0.0);
    float ggx2  = geometrySchlickGGX(NdotV, roughness);
    float ggx1  = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

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
    vec3 H = normalize(lightDir + V);

    if (!gl_FrontFacing) {
        // Flip normal for back faces
        normal = -normal;
    }

    // Base reflectivity
    vec3 specColor = mix(F0.rrr, material.baseColor.rgb, material.metalness);

    // Cook-Torrance BRDF
    float NDF = distributionGGX(normal, H, material.roughness);
    float G   = geometrySmith(normal, V, lightDir, material.roughness);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), specColor);

    // Specular and diffuse components
    float denominator = 4.0 * max(dot(normal, V), dot(normal, lightDir)) + 0.0001;
    vec3 specular     = NDF * G * F / denominator;

    // Energy conservation: diffuse and specular
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;

    // Only non-metallic surfaces have diffuse lighting
    kD *= 1.0 - material.metalness;

    // Combine lighting (no attenuation for directional light)
    float NdotL = max(dot(normal, lightDir), 0.0);
    const vec3 lightCol = light.intensity * light.color;
    vec3 Lo = (kD * material.baseColor.rgb / PI + specular) * lightCol * NdotL;

#ifdef HAS_SHADOW_MAPS
    float shadowValue = calcShadowmap(NdotL);
    Lo = shadowValue * Lo;
#endif

    // Ambient term (very simple)
    vec3 ambient = vec3(0.03) * material.baseColor.rgb * material.ao;
#ifdef HAS_SSAO
    vec2 ssaoTexSize = textureSize(ssaoTex, 0).xy;
    vec2 ssaoUV;
    ssaoUV = gl_FragCoord.xy / ssaoTexSize;
    if(params.useSsao == 1) {
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
