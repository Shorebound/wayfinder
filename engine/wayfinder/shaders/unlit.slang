import "modules/transforms";

[[vk::binding(0, 1)]]
ConstantBuffer<UnlitTransformData> transform;

struct MaterialParams
{
    float4 base_colour;
};
[[vk::binding(0, 3)]]
ConstantBuffer<MaterialParams> material;

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Colour : TEXCOORD0;
};

struct VSInput
{
    float3 Position : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float3 Colour : TEXCOORD2;
};

[shader("vertex")]
VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = mul(transform.mvp, float4(input.Position, 1.0));
    output.Colour = input.Colour;
    return output;
}

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_Target
{
    return float4(input.Colour * material.base_colour.rgb, material.base_colour.a);
}
