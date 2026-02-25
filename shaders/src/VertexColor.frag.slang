[shader("fragment")]
float4 main([vk::location(0)] float4 interpColor) : SV_Target0 {
    return interpColor;
}
