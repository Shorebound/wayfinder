// Colour grading: exposure (stops), LGG, contrast, saturation (from legacy composition.frag).
// Compiled with DXC: dxc -T ps_6_0 -E PSMain -spirv colour_grading.frag -Fo colour_grading.frag.spv

[[vk::binding(0, 3)]]
cbuffer ColourGradingParams : register(b0, space3)
{
    float4 ExposureContrastSaturationPad;
    float4 Lift;
    float4 Gamma;
    float4 Gain;
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

    c *= exp2(ExposureContrastSaturationPad.x);
    c += Lift.rgb;
    c *= Gain.rgb;
    c = pow(max(c, float3(1e-5, 1e-5, 1e-5)), 1.0 / max(Gamma.rgb, float3(1e-5, 1e-5, 1e-5)));
    c = (c - 0.5) * ExposureContrastSaturationPad.y + 0.5;
    float luma = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = lerp(float3(luma, luma, luma), c, ExposureContrastSaturationPad.z);

    return float4(c, 1.0);
}
