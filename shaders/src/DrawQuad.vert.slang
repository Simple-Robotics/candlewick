// Vertex shader to render a quad on screen.
// Outputs the corresponding UV coordinates,
// which get interpolated in the fragment shader.

static const float2 positions[6] = {
    float2(-1.0, -1.0),
    float2( 1.0, -1.0),
    float2( 1.0,  1.0),
    float2(-1.0, -1.0),
    float2( 1.0,  1.0),
    float2(-1.0,  1.0)
};

struct VSOutput {
    [vk::location(0)] float2 outUV;
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput main(uint vertexID : SV_VertexID) {
    float2 pos = positions[vertexID];
    VSOutput output;
    output.position = float4(pos, 0.0, 1.0);
    // Convert to UV coordinates
    output.outUV.x = pos.x * 0.5 + 0.5;
    output.outUV.y = 0.5 - pos.y * 0.5;
    return output;
}
