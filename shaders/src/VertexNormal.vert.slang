struct TranformBlock {
    float4x4 mvp;
    float3x3 normalMatrix;
};

[vk::binding(0, 1)] ConstantBuffer<TranformBlock> transform;

struct VSOutput {
    [vk::location(0)] float3 f_normal;
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput main([vk::location(0)] float3 position,
              [vk::location(1)] float3 normal) {
    VSOutput output;
    output.f_normal = normalize(mul(transform.normalMatrix, normal));
    output.position = mul(transform.mvp, float4(position, 1.0));
    return output;
}
