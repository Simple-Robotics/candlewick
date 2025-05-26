// See https://64.github.io/tonemapping/

// Reinhard Tone Mapping
vec3 reinhardToneMapping(vec3 color) {
    return color / (color + vec3(1.0));
}

vec3 exposureToneMapping(vec3 color, float exposure) {
    // Exposure adjustment
    return 1.0 - exp(-color * exposure);
}

// ACES Filmic Tone Mapping (Academy Color Encoding System)
vec3 acesFastToneMapping(vec3 color) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;

    return clamp(
        (color * (a * color + vec3(b))) /
        (color * (c * color + vec3(d)) + vec3(e)),
        vec3(0.0),
        vec3(1.0)
    );
}

// Uncharted 2 Tone Mapping (John Hable)
vec3 uncharted2ToneMapping_Partial(vec3 color) {
    float A = 0.15f;  // Shoulder strength
    float B = 0.50f;  // Linear strength
    float C = 0.10f;  // Linear angle
    float D = 0.20f;  // Toe strength
    float E = 0.02f;  // Toe numerator
    float F = 0.30f;  // Toe denominator

    return (color * (A * color + C * B) + D * E) /
           (color * (A * color + B) + D * F) - vec3(E / F);
}

vec3 uncharted2ToneMapping(vec3 color) {
    float exposure_bias = 2.0;
    vec3 curr = uncharted2ToneMapping_Partial(exposure_bias * color);

    vec3 W = vec3(11.2f);  // White point
    vec3 white_scale = vec3(1.0) / uncharted2ToneMapping_Partial(W);
    return curr * white_scale;
}

vec3 saturate(vec3 color) {
    return clamp(color, vec3(0.0), vec3(1.0));
}
