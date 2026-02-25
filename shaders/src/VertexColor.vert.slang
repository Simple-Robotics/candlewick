struct UniformBlock {
    float4x4 viewProj;
};

[vk::binding(0, 1)] ConstantBuffer<UniformBlock> transform;

struct VSOutput {
    [vk::location(0)] float4 interpColor;
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput main([vk::location(0)] float3 position,
              [vk::location(4)] float4 color) {
    VSOutput output;
    output.position = mul(transform.viewProj, float4(position, 1.0));
    output.interpColor = color;
    return output;
}
