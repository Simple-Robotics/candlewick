#version 450

layout(location=0) in vec2 inUV;
layout(location=0) out float aoValue;

layout(set=2, binding=0) uniform sampler2D aoTex;


const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
const int KERNEL_RADIUS = 4;

layout(set=3, binding=0) uniform BlurParams
{
    vec2 direction;
};

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(aoTex, 0));
    float result = texture(aoTex, inUV).r * weights[0];

    for(int i = 1; i <= KERNEL_RADIUS; i++) {
        vec2 off = direction * texelSize * i;
        result += texture(aoTex, inUV + off).r * weights[i];
        result += texture(aoTex, inUV - off).r * weights[i];
    }
    aoValue = result;
}
