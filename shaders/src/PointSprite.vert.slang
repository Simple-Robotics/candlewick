import tone_mapping;

struct TranformBlock {
    float4x4 model;
    float4x4 viewProj;
};

[vk::binding(0, 1)] ConstantBuffer<TranformBlock> transform;

struct VSOutput {
    [vk::location(0)] float4 pointColor;
    float4 position : SV_Position;
    float pointSize : SV_PointSize;
};

[shader("vertex")]
VSOutput main([vk::location(0)] float3 position, [vk::location(4)] float4 color) {
    VSOutput output;
    output.position = mul(transform.viewProj, mul(transform.model, float4(position, 1.0)));
    output.pointSize = 5.0;
    output.pointColor = float4(uncharted2ToneMapping(color.rgb), 1.0);
    return output;
}
