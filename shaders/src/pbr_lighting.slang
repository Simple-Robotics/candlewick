import config;

struct LightBlock {
    float3 direction[MAX_NUM_LIGHTS];
    float3 color[MAX_NUM_LIGHTS];
    float intensity[MAX_NUM_LIGHTS];
    int numLights;
    uint useSsao;
};

struct PbrMaterial {
    float4 baseColor;
    float metalness;
    float roughness;
    float ao;
};

static const float PI = 3.14159265359;
static const float BASE_REFLECTIVITY = 0.04;

// Schlick's Fresnel approximation
float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float distributionGGX(float3 normal, float3 H, float roughness) {
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
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(float3 normal, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(normal, V), 0.0);
    float NdotL = max(dot(normal, L), 0.0);
    float ggx2  = geometrySchlickGGX(NdotV, roughness);
    float ggx1  = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Core PBR calculation - returns direct lighting only
// (no ambient, no shadows, no SSAO - those are applied outside)
float3 calculatePbrLighting(
    float3 normal,
    float3 V,
    float3 lightDir,
    PbrMaterial material,
    float3 lightColor,
    float lightIntensity
) {
    float3 H = normalize(lightDir + V);

    float3 specColor = lerp(float3(BASE_REFLECTIVITY), material.baseColor.rgb, material.metalness);

    float NDF = distributionGGX(normal, H, material.roughness);
    float G   = geometrySmith(normal, V, lightDir, material.roughness);
    float3 F  = fresnelSchlick(max(dot(H, V), 0.0), specColor);

    float denominator = 4.0 * max(dot(normal, V), dot(normal, lightDir)) + 0.0001;
    float3 specular   = NDF * G * F / denominator;

    float3 kS = F;
    float3 kD = float3(1.0) - kS;
    kD *= 1.0 - material.metalness;

    float NdotL = max(dot(normal, lightDir), 0.0);
    float3 lightCol = lightIntensity * lightColor;

    return (kD * material.baseColor.rgb / PI + specular) * lightCol * NdotL;
}
