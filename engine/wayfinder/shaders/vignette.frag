// Vignette: screen-edge darkening (from legacy composition.frag).
// Compiled with DXC: dxc -T ps_6_0 -E PSMain -spirv vignette.frag -Fo vignette.frag.spv

[[vk::binding(0, 3)]]
cbuffer VignetteParams : register(b0, space3)
{
    float VignetteStrength;
    float3 _pad;
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
    float3 c = SceneColourTex.Sample(PointSampler, uv).rgb;

    float2 vigUv = uv * 2.0 - 1.0;
    float vig = saturate(1.0 - dot(vigUv, vigUv) * VignetteStrength);
    c *= vig;

    return float4(c, 1.0);
}
