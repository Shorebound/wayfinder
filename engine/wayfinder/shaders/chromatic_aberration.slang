import "modules/fullscreen";

struct ChromaticAberrationParams
{
    float4 IntensityPad;
};
[[vk::binding(0, 3)]]
ConstantBuffer<ChromaticAberrationParams> chromaticParams;

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
    float ab = chromaticParams.IntensityPad.x * length(uv - 0.5);
    float2 dir = uv - 0.5;
    float2 off = dir * ab * 0.02;
    float r = SceneColourTex.Sample(uv - off).r;
    float g = SceneColourTex.Sample(uv).g;
    float b = SceneColourTex.Sample(uv + off).b;
    return float4(r, g, b, 1.0);
}
