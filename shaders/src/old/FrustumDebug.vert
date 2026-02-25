#version 450

layout(set=1, binding=0) uniform DebugTransform {
   mat4 invProj;
   mat4 mvp;
   vec4 color;
   vec3 eyePos;
};

layout(location=0) out vec4 outColor;

// Line indices for frustum edges
const uint[24] LINE_INDICES = uint[](
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
);

vec4 getNDCCorner(uint idx) {
    return vec4(
        2.0 * float(idx & 1) - 1.0,
        2.0 * float((idx >> 1) & 1) - 1.0,
        2.0 * float((idx >> 2) & 1) - 1.0,
        1.0
    );
}

void main() {
    if (gl_VertexIndex < 24) {
        uint cornerIdx = LINE_INDICES[gl_VertexIndex];
        vec4 ndc = getNDCCorner(cornerIdx);
        vec4 viewPos = invProj * ndc;
        viewPos /= viewPos.w;
        gl_Position = mvp * viewPos;
    } else if (gl_VertexIndex < 30) {
        uint axisIdx = (gl_VertexIndex - 24) / 2;
        uint pointIdx = gl_VertexIndex & 1;
        float size = 0.1;
        vec3 offset = vec3(0.0);
        offset[axisIdx] = pointIdx == 0 ? -size : size;
        gl_Position = mvp * vec4(eyePos + offset, 1.0);
    } else {
        // Z=0 intersection quad
        uint lineIdx = (gl_VertexIndex - 30) / 2;
        uint pointIdx = gl_VertexIndex & 1;

        uint edgeNum = (lineIdx + pointIdx) % 4;
        uint near = edgeNum;
        uint far = near + 4;
        vec4 center = mix(getNDCCorner(near), getNDCCorner(far), 0.5);

        vec4 viewPos = invProj * center;
        viewPos /= viewPos.w;
        gl_Position = mvp * viewPos;
    }
    outColor = color;
}
