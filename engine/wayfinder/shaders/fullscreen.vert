// Wayfinder Fullscreen Triangle Vertex Shader
// Uses SV_VertexID to generate a fullscreen triangle — no vertex buffer needed.
// Pipeline should use an empty vertex layout and DrawPrimitives(3).
// Compiled with DXC: dxc -T vs_6_0 -E VSMain -spirv fullscreen.vert -Fo fullscreen.vert.spv

struct VSOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
    VSOutput output;
    // Generate 3 vertices covering the full screen as a single triangle
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.Position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    // Flip V: Vulkan framebuffer Y is top-down, but the scene projection is Y-up.
    output.TexCoord = float2(uv.x, 1.0 - uv.y);
    return output;
}
