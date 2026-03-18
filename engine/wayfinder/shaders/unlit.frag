// Wayfinder Unlit Fragment Shader
// Compiled with DXC: dxc -T ps_6_0 -E PSMain -spirv unlit.frag -Fo unlit.frag.spv

struct PSInput
{
    float4 Position : SV_Position;
    float3 Color    : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_Target
{
    return float4(input.Color, 1.0);
}
