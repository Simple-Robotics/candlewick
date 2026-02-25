#version 450

layout(location=0) in vec3 inPosition;

layout(set=1, binding=0) uniform CameraBlock {
    mat4 mvp;
};

out gl_PerVertex {
    invariant vec4 gl_Position;
};

void main() {
    gl_Position = mvp * vec4(inPosition, 1.0);
}
