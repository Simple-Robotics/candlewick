[shader("fragment")]
float main(float4 fragCoord : SV_Position) : SV_Depth {
    return fragCoord.z;
}
