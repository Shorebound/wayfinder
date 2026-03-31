// Self-contained shader mirroring engine textured_lit binding counts (fixture-only).

struct TransformData
{
    float4x4 mvp;
    float4x4 model;
};
[[vk::binding(0, 1)]]
ConstantBuffer<TransformData> transform;

[[vk::binding(0, 2)]]
Sampler2D<float4> DiffuseTex;

struct MaterialParams
{
    float4 base_colour;
};
[[vk::binding(0, 3)]]
ConstantBuffer<MaterialParams> material;

struct SceneGlobals
{
    float4 a;
    float4 b;
    float4 c;
    float4 d;
};
[[vk::binding(1, 3)]]
ConstantBuffer<SceneGlobals> globals;

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Normal : TEXCOORD0;
    float2 UV : TEXCOORD1;
};

struct VSInput
{
    float3 Position : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 UV : TEXCOORD2;
    float4 Tangent : TEXCOORD3;
};

[shader("vertex")]
VSOutput VSMain(VSInput input)
{
    VSOutput o;
    o.Position = mul(transform.mvp, float4(input.Position, 1.0));
    o.Normal = input.Normal;
    o.UV = input.UV;
    return o;
}

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_Target
{
    return DiffuseTex.Sample(input.UV) * material.base_colour * globals.a;
}
