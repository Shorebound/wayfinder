// Shader that imports a module, for testing module resolution.

import "modules/test_module";

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Colour : TEXCOORD0;
};

struct VSInput
{
    float3 Position : TEXCOORD0;
};

[shader("vertex")]
VSOutput VSMain(VSInput input)
{
    TestData data;
    data.value = input.Position;
    data.Scale(1.0);

    VSOutput output;
    output.Position = float4(data.value, 1.0);
    output.Colour = float3(1.0, 0.0, 0.0);
    return output;
}

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_Target
{
    return float4(input.Colour, 1.0);
}
