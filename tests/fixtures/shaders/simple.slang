// Minimal shader for testing SlangCompiler.
// Contains both vertex and fragment entry points.

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Colour : TEXCOORD0;
};

struct VSInput
{
    float3 Position : TEXCOORD0;
    float3 Colour : TEXCOORD1;
};

[shader("vertex")]
VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = float4(input.Position, 1.0);
    output.Colour = input.Colour;
    return output;
}

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_Target
{
    return float4(input.Colour, 1.0);
}
