[vk::binding(0, 2)] Sampler2D aoTex;

static const float weights[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };
static const int KERNEL_RADIUS = 4;

struct BlurParams {
    float2 direction;
};

[vk::binding(0, 3)] ConstantBuffer<BlurParams> blur;

[shader("fragment")]
float main([vk::location(0)] float2 inUV) : SV_Target0 {
    uint w, h;
    aoTex.GetDimensions(w, h);
    float2 texelSize = 1.0 / float2(w, h);
    float result = aoTex.Sample(inUV).r * weights[0];

    for (int i = 1; i <= KERNEL_RADIUS; i++) {
        float2 off = blur.direction * texelSize * i;
        result += aoTex.Sample(inUV + off).r * weights[i];
        result += aoTex.Sample(inUV - off).r * weights[i];
    }
    return result;
}
