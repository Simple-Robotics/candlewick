struct TranformBlock {
    float4x4 mvp;
};

[vk::binding(0, 1)] ConstantBuffer<TranformBlock> transform;

[shader("vertex")]
float4 main([vk::location(0)] float3 inPosition) : SV_Position {
    return mul(transform.mvp, float4(inPosition, 1.0));
}
