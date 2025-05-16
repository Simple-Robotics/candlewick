#ifndef _UTILS_GLSL_
#define _UTILS_GLSL_


bool isCoordsInRange(vec3 uv) {
    return uv.x >= 0.0 &&
           uv.y >= 0.0 &&
           uv.x <= 1.0 &&
           uv.y <= 1.0 &&
           uv.z >= 0.0 &&
           uv.z <= 1.0;
}

bool isCoordsInRange(vec2 uv) {
    return uv.x >= 0.0 &&
           uv.y >= 0.0 &&
           uv.x <= 1.0 &&
           uv.y <= 1.0;
}

#endif // _UTILS_GLSL_
