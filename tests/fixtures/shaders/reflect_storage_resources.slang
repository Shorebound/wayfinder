// Fixture exercising storage buffer and storage texture reflection.

[[vk::binding(0, 0)]]
RWStructuredBuffer<float4> VertexStorageBuffer;

[[vk::binding(1, 0)]]
RWTexture2D<float4> VertexStorageTexture;

struct VSOutput
{
    float4 Position : SV_Position;
    float4 Colour : TEXCOORD0;
};

[shader("vertex")]
VSOutput VSMain(uint vertexId : SV_VertexID)
{
    const float4 bufferValue = VertexStorageBuffer[vertexId];
    const float4 textureValue = VertexStorageTexture[int2(int(vertexId), 0)];

    VSOutput output;
    output.Position = float4(bufferValue.xy, 0.0, 1.0);
    output.Colour = bufferValue + textureValue;
    return output;
}

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_Target
{
    const float4 textureValue = VertexStorageTexture[int2(0, 0)];
    const float4 bufferValue = VertexStorageBuffer[0];
    return input.Colour + bufferValue + textureValue;
}