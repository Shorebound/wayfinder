// Chromatic aberration: radial RGB channel separation (from legacy composition.frag).
// Compiled with DXC: dxc -T ps_6_0 -E PSMain -spirv chromatic_aberration.frag -Fo chromatic_aberration.frag.spv

[[vk::binding(0, 3)]]
cbuffer ChromaticAberrationParams : register(b0, space3)
{
    float4 IntensityPad; // x = intensity
};

[[vk::combinedImageSampler]]
[[vk::binding(0, 2)]]
Texture2D<float4> SceneColourTex : register(t0, space2);

[[vk::combinedImageSampler]]
[[vk::binding(0, 2)]]
SamplerState PointSampler : register(s0, space2);

struct PSInput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_Target
{
    float2 uv = input.TexCoord;
    float ab = IntensityPad.x * length(uv - 0.5);
    float2 dir = uv - 0.5;
    float2 off = dir * ab * 0.02;
    float r = SceneColourTex.Sample(PointSampler, uv - off).r;
    float g = SceneColourTex.Sample(PointSampler, uv).g;
    float b = SceneColourTex.Sample(PointSampler, uv + off).b;
    return float4(r, g, b, 1.0);
}
