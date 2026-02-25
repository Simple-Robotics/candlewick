struct VSOutput {
    [vk::location(0)] float4 outColor;
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput main(uint vertexID : SV_VertexID) {
    VSOutput output;
    float2 pos;
    if (vertexID == 0) {
        pos = float2(-1, -1);
        output.outColor = float4(1, 0, 0, 1);
    } else if (vertexID == 1) {
        pos = float2(1, -1);
        output.outColor = float4(0, 1, 0, 1);
    } else {
        pos = float2(0, 1);
        output.outColor = float4(0, 0, 1, 1);
    }
    output.position = float4(pos, 0, 1);
    return output;
}
