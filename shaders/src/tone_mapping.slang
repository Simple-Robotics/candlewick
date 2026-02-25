// See https://64.github.io/tonemapping/

float3 reinhardToneMapping(float3 color) {
    return color / (color + float3(1.0));
}

float3 exposureToneMapping(float3 color, float exposure) {
    return 1.0 - exp(-color * exposure);
}

float3 acesFastToneMapping(float3 color) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate(
        (color * (a * color + float3(b))) /
        (color * (c * color + float3(d)) + float3(e))
    );
}

float3 uncharted2ToneMapping_Partial(float3 color) {
    float A = 0.15;  // Shoulder strength
    float B = 0.50;  // Linear strength
    float C = 0.10;  // Linear angle
    float D = 0.20;  // Toe strength
    float E = 0.02;  // Toe numerator
    float F = 0.30;  // Toe denominator
    return (color * (A * color + C * B) + D * E) /
           (color * (A * color + B) + D * F) - float3(E / F);
}

float3 uncharted2ToneMapping(float3 color) {
    float exposure_bias = 2.0;
    float3 curr = uncharted2ToneMapping_Partial(exposure_bias * color);
    float3 W = float3(11.2);  // White point
    float3 white_scale = float3(1.0) / uncharted2ToneMapping_Partial(W);
    return curr * white_scale;
}
