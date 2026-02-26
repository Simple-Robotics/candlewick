[vk::binding(0, 2)] Sampler2DMS<float4> accumTexture;
[vk::binding(1, 2)] Sampler2DMS<float1> revealTexture;

[shader("fragment")]
float4 main(float4 fragCoord : SV_Position,
            uint sampleIndex : SV_SampleIndex) : SV_Target0 {
    int2 uv = int2(fragCoord.xy);

    float4 accum  = accumTexture.Load(uv, sampleIndex);
    float  reveal = revealTexture.Load(uv, sampleIndex).r;

    float3 color = accum.rgb;
    if (accum.a > 0.001) {
        color = accum.rgb / accum.a;
    }

    return float4(color, 1.0 - reveal);
}
