#version 450
layout(set=2, binding=0) uniform sampler2DMS accumTexture;
layout(set=2, binding=1) uniform sampler2DMS revealTexture;

layout(location = 0) out vec4 outColor;

void main() {
    ivec2 uv = ivec2(gl_FragCoord.xy);

    vec4 accum   = texelFetch(accumTexture, uv, gl_SampleID);
    float reveal = texelFetch(revealTexture, uv, gl_SampleID).r;

    vec3 color = accum.rgb;
    if (accum.a > 0.001) {
        color = accum.rgb / accum.a;
    }

    // Output final color
    outColor = vec4(color, reveal);
}
