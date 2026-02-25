[shader("fragment")]
float4 main([vk::location(0)] float4 pointColor,
            float2 pointCoord : SV_PointCoord) : SV_Target0 {
    if (length(pointCoord - 0.5) > 0.5) discard;
    return pointColor;
}
