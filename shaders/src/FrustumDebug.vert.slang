struct DebugTransform {
    float4x4 invProj;
    float4x4 mvp;
    float4 color;
    float3 eyePos;
};

[vk::binding(0, 1)] ConstantBuffer<DebugTransform> transform;

// Line indices for frustum edges
static const uint LINE_INDICES[24] = {
    // bottom face
    0, 1,  // x-edge
    0, 2,  // y-edge
    2, 3,  // x-edge
    1, 3,  // y-edge
    // top face
    4, 5,  // x-edge
    4, 6,  // y-edge
    6, 7,  // x-edge
    5, 7,  // y-edge
    // vertical edges
    0, 4,  // z-edge
    1, 5,  // z-edge
    2, 6,  // z-edge
    3, 7   // z-edge
};

float4 getNDCCorner(uint idx) {
    return float4(
        2.0 * float(idx & 1) - 1.0,
        2.0 * float((idx >> 1) & 1) - 1.0,
        2.0 * float((idx >> 2) & 1) - 1.0,
        1.0
    );
}

struct VSOutput {
    [vk::location(0)] float4 outColor;
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput main(uint vertexID : SV_VertexID) {
    VSOutput output;
    output.outColor = transform.color;

    if (vertexID < 24) {
        uint cornerIdx = LINE_INDICES[vertexID];
        float4 ndc = getNDCCorner(cornerIdx);
        float4 viewPos = mul(transform.invProj, ndc);
        viewPos /= viewPos.w;
        output.position = mul(transform.mvp, viewPos);
    } else if (vertexID < 30) {
        uint axisIdx = (vertexID - 24) / 2;
        uint pointIdx = vertexID & 1;
        float size = 0.1;
        float3 offset = float3(0.0);
        offset[axisIdx] = (pointIdx == 0) ? -size : size;
        output.position = mul(transform.mvp, float4(transform.eyePos + offset, 1.0));
    } else {
        // Z=0 intersection quad
        uint lineIdx = (vertexID - 30) / 2;
        uint pointIdx = vertexID & 1;
        uint edgeNum = (lineIdx + pointIdx) % 4;
        uint near = edgeNum;
        uint far = near + 4;
        float4 center = lerp(getNDCCorner(near), getNDCCorner(far), 0.5);
        float4 viewPos = mul(transform.invProj, center);
        viewPos /= viewPos.w;
        output.position = mul(transform.mvp, viewPos);
    }
    return output;
}
