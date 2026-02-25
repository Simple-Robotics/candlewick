bool isCoordsInRange(float3 uv) {
    return uv.x >= 0.0 && uv.y >= 0.0 &&
           uv.x <= 1.0 && uv.y <= 1.0 &&
           uv.z >= 0.0 && uv.z <= 1.0;
}

bool isCoordsInRange(float2 uv) {
    return uv.x >= 0.0 && uv.y >= 0.0 &&
           uv.x <= 1.0 && uv.y <= 1.0;
}

float linearizeDepth(float depth, float zNear, float zFar) {
    float z_ndc = 2.0 * depth - 1.0;
    return (2.0 * zNear * zFar) / (zFar + zNear - z_ndc * (zFar - zNear));
}

float linearizeDepthOrtho(float depth, float zNear, float zFar) {
    return zNear + depth * (zFar - zNear);
}
