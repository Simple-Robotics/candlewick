struct Color {
    float4 color;
};

[vk::binding(0, 3)] ConstantBuffer<Color> colorBlock;

[shader("fragment")]
float4 main() : SV_Target0 {
    return colorBlock.color;
}
