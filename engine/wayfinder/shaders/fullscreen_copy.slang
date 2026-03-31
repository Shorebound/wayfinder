import "modules/fullscreen";

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
    return SceneColourTex.Sample(input.TexCoord);
}
