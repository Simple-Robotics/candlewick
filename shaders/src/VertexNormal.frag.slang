[shader("fragment")]
float4 main([vk::location(0)] float3 f_normal) : SV_Target0 {
    return float4(f_normal, 0.0);
}
