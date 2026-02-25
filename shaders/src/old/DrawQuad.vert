// Vertex shader to render a quad on screen.
// Outputs the corresponding UV coordinates,
// which get interpolated in the fragment shader.
#version 450

const vec2 positions[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

layout(location = 0) out vec2 outUV;

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);
    // Convert to UV coordinates
    outUV.x = pos.x * 0.5 + 0.5;
    outUV.y = 0.5 - pos.y * 0.5;
}
