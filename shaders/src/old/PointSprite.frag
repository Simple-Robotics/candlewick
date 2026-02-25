#version 450

layout(location=0) in vec4 pointColor;
layout(location=0) out vec4 fragColor;

void main() {
    // Can use gl_PointCoord for texture/circular shape
    // Can discard fragments to make round points
    if (length(gl_PointCoord - 0.5) > 0.5) {
        discard;
    }
    fragColor = pointColor;
}
