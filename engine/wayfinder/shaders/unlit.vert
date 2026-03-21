// Wayfinder Unlit Vertex Shader (PosNormalColour format)
// Scene objects use PosNormalColour; Normal is ignored for unlit rendering.
// Compiled with DXC: dxc -T vs_6_0 -E VSMain -spirv unlit.vert -Fo unlit.vert.spv
// SDL_GPU SPIR-V convention: vertex UBO at set 1

[[vk::binding(0, 1)]]
cbuffer UBO : register(b0)
{
    float4x4 mvp;
};

struct VSInput
{
    float3 Position : TEXCOORD0;
    float3 Normal   : TEXCOORD1; // Present but unused for unlit
    float3 Colour    : TEXCOORD2;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Colour    : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = mul(mvp, float4(input.Position, 1.0));
    output.Colour = input.Colour;
    return output;
}
