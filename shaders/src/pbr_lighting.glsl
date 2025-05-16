#ifndef _PBR_LIGHTING_GLSL_
#define _PBR_LIGHTING_GLSL_

struct PbrMaterial {
    vec4 baseColor;
    float metalness;
    float roughness;
    float ao;
};

// Constants
const float PI = 3.14159265359;
const float F0 = 0.04; // Standard base reflectivity

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

// Core PBR calculation - returns direct lighting only
// (no ambient, no shadows, no SSAO - those are applied outside)
vec3 calculatePbrLighting(
    vec3 normal,
    vec3 V,
    vec3 lightDir,
    PbrMaterial material,
    vec3 lightColor,
    float lightIntensity
) {
    vec3 H = normalize(lightDir + V);

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
    const vec3 lightCol = lightIntensity * lightColor;

    // Return direct lighting contribution
    return (kD * material.baseColor.rgb / PI + specular) * lightCol * NdotL;
}

#endif // _PBR_LIGHTING_GLSL_
