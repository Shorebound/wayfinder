// Wayfinder Unlit Fragment Shader
// Compiled with DXC: dxc -T ps_6_0 -E PSMain -spirv unlit.frag -Fo unlit.frag.spv
// SDL_GPU SPIR-V convention: fragment UBO at set 3

[[vk::binding(0, 3)]]
cbuffer MaterialUBO : register(b0)
{
    float4 base_color;
};

struct PSInput
{
    float4 Position : SV_Position;
    float3 Color    : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_Target
{
    return float4(input.Color * base_color.rgb, base_color.a);
}
