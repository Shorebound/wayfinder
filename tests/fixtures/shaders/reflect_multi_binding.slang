// Fixture exercising parameter-block reflection across multiple binding ranges.

struct VertexResources
{
    float4x4 Mvp;
    Sampler2D<float4> WindTex;
};

struct FragmentResources
{
    float4 Tint;
    Sampler2D<float4> ColourTex;
};

[[vk::binding(0, 1)]]
ParameterBlock<VertexResources> VertexBlock;

[[vk::binding(0, 2)]]
ParameterBlock<FragmentResources> FragmentBlock;

struct VSInput
{
    float3 Position : TEXCOORD0;
    float2 UV : TEXCOORD1;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

[shader("vertex")]
VSOutput VSMain(VSInput input)
{
    VSOutput output;
    const float4 windSample = VertexBlock.WindTex.SampleLevel(input.UV, 0.0);
    const float3 displacedPosition = input.Position + float3(windSample.x * 0.01, 0.0, 0.0);
    output.Position = mul(VertexBlock.Mvp, float4(displacedPosition, 1.0));
    output.UV = input.UV;
    return output;
}

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_Target
{
    return FragmentBlock.ColourTex.Sample(input.UV) * FragmentBlock.Tint;
}