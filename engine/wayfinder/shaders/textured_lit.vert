// Wayfinder Textured Lit Vertex Shader
// Uses VertexPosNormalUV format (Position, Normal, UV).
// Compiled with DXC: dxc -T vs_6_0 -E VSMain -spirv textured_lit.vert -Fo textured_lit.vert.spv
// SDL_GPU SPIR-V convention: vertex UBO at set 1

[[vk::binding(0, 1)]]
cbuffer TransformUBO : register(b0)
{
    float4x4 mvp;
    float4x4 model;
};

struct VSInput
{
    float3 Position : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float2 UV       : TEXCOORD2;
};

struct VSOutput
{
    float4 Position  : SV_Position;
    float3 Normal    : TEXCOORD0;
    float2 UV        : TEXCOORD1;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = mul(mvp, float4(input.Position, 1.0));
    output.Normal = normalize(mul((float3x3)model, input.Normal));
    output.UV = input.UV;
    return output;
}
