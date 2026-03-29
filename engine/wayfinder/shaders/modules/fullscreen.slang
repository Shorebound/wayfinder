module fullscreen;

public struct FullscreenOutput
{
    public float4 Position : SV_Position;
    public float2 TexCoord : TEXCOORD0;
};

/// Fullscreen triangle from SV_VertexID (no vertex buffer). Pipeline: empty layout, DrawPrimitives(3).
public FullscreenOutput FullscreenTriangle(uint vertexId)
{
    FullscreenOutput output;
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.Position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    output.TexCoord = float2(uv.x, 1.0 - uv.y);
    return output;
}
