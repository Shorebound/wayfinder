import "modules/fullscreen";

struct ColourGradingParams
{
    float4 ExposureContrastSaturationPad;
    float4 Lift;
    float4 Gamma;
    float4 Gain;
};
[[vk::binding(0, 3)]]
ConstantBuffer<ColourGradingParams> grading;

[[vk::binding(0, 2)]]
Sampler2D<float4> SceneColourTex;

[shader("vertex")]
FullscreenOutput VSMain(uint vertexId : SV_VertexID)
{
    return FullscreenTriangle(vertexId);
}

[shader("fragment")]
float4 PSMain(FullscreenOutput input) : SV_Target
{
    float2 uv = input.TexCoord;
    float3 c = SceneColourTex.Sample(uv).rgb;

    c *= exp2(grading.ExposureContrastSaturationPad.x);
    c += grading.Lift.rgb;
    c *= grading.Gain.rgb;
    c = pow(max(c, float3(1e-5, 1e-5, 1e-5)), 1.0 / max(grading.Gamma.rgb, float3(1e-5, 1e-5, 1e-5)));
    c = (c - 0.5) * grading.ExposureContrastSaturationPad.y + 0.5;
    float luma = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = lerp(float3(luma, luma, luma), c, grading.ExposureContrastSaturationPad.z);

    return float4(c, 1.0);
}
