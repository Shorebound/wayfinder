// Fullscreen-style blit: no vertex resources, one combined sampler in fragment.

[[vk::binding(0, 2)]]
Sampler2D<float4> SceneColourTex;

[shader("vertex")]
float4 VSMain(uint vertexId : SV_VertexID) : SV_Position
{
    return float4(0.0, 0.0, 0.0, 1.0);
}

[shader("fragment")]
float4 PSMain(float4 pos : SV_Position) : SV_Target
{
    return SceneColourTex.Sample(float2(0.0, 0.0));
}
