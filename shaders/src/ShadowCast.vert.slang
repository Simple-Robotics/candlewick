struct CameraBlock {
    float4x4 mvp;
};

[vk::binding(0, 1)] ConstantBuffer<CameraBlock> camera;

[shader("vertex")]
float4 main([vk::location(0)] float3 inPosition) : SV_Position {
    return mul(camera.mvp, float4(inPosition, 1.0));
}
