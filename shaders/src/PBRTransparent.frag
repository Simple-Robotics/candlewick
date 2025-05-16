#version 450
// layout declarations with two outputs
layout(location = 0) out vec4 accum; // R16G16B16A16 accumulation
layout(location = 1) out float reveal; // R8 revealage

void main() {
    // Normal PBR calculation to get final_color
    vec4 color = ... // your existing PBR calculation

    // WBOIT weight calculation - this is what makes it work well
    float weight = clamp(pow(min(1.0, color.a * 10.0) + 0.01, 3.0) * 1e8 *
                         pow(1.0 - gl_FragCoord.z * 0.9, 3.0), 1e-2, 3e3);

    // Output weighted color to accumulation buffer
    accum = vec4(color.rgb * color.a, color.a) * weight;

    // Output to revealage buffer
    reveal = color.a;
}
