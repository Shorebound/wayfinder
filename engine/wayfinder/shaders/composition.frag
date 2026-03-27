// Wayfinder composition fragment: sample PresentSource / scene colour, apply grading, write swapchain.
// Compiled with DXC: dxc -T ps_6_0 -E PSMain -spirv composition.frag -Fo composition.frag.spv

cbuffer CompositionParams : register(b0, space0)
{
    float4 ExposureContrastSaturationPad;
    float4 Lift;
    float4 Gamma;
    float4 Gain;
    float4 VignetteAberrationPad;
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
    float ab = VignetteAberrationPad.y * length(uv - 0.5);
    float2 dir = uv - 0.5;
    float2 off = dir * ab * 0.02;
    float r = SceneColourTex.Sample(PointSampler, uv - off).r;
    float g = SceneColourTex.Sample(PointSampler, uv).g;
    float b = SceneColourTex.Sample(PointSampler, uv + off).b;
    float3 c = float3(r, g, b);

    c *= exp2(ExposureContrastSaturationPad.x);
    c += Lift.rgb;
    c *= Gain.rgb;
    c = pow(max(c, float3(1e-5, 1e-5, 1e-5)), 1.0 / max(Gamma.rgb, float3(1e-5, 1e-5, 1e-5)));
    c = (c - 0.5) * ExposureContrastSaturationPad.y + 0.5;
    const float luma = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = lerp(float3(luma, luma, luma), c, ExposureContrastSaturationPad.z);

    float2 vigUv = uv * 2.0 - 1.0;
    const float vig = saturate(1.0 - dot(vigUv, vigUv) * VignetteAberrationPad.x);
    c *= vig;

    return float4(c, 1.0);
}
