#version 450
layout(set=2, binding=0) uniform sampler2D accumTexture;
layout(set=2, binding=1) uniform sampler2D revealTexture;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 viewportSize = textureSize(accumTexture, 0).xy;
    vec2 uv = gl_FragCoord.xy / viewportSize;

    vec4 accum = texture(accumTexture, uv);
    float reveal = texture(revealTexture, uv).r;

    vec3 color = accum.rgb;
    if (accum.a > 0.001) {
        color = accum.rgb / accum.a;
    }

    // Output final color
    outColor = vec4(color, reveal);
}
