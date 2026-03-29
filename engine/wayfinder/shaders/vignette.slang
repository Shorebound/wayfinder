import "modules/fullscreen";

struct VignetteParams
{
    float VignetteStrength;
    float3 _pad;
};
[[vk::binding(0, 3)]]
ConstantBuffer<VignetteParams> vignette;

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

    float2 vigUv = uv * 2.0 - 1.0;
    float vig = saturate(1.0 - dot(vigUv, vigUv) * vignette.VignetteStrength);
    c *= vig;

    return float4(c, 1.0);
}
