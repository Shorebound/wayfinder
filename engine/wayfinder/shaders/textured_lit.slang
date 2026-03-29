import "modules/transforms";
import "modules/scene_globals";

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
    VSOutput output;
    output.Position = mul(transform.mvp, float4(input.Position, 1.0));
    output.Normal = normalize(mul(NormalMatrixFromModel(transform.model), input.Normal));
    output.UV = input.UV;
    return output;
}

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_Target
{
    float4 texColour = DiffuseTex.Sample(input.UV);

    float3 N = normalize(input.Normal);
    float3 L = normalize(-globals.light_direction);
    float NdotL = max(dot(N, L), 0.0);

    float3 diffuse = globals.light_colour * globals.light_intensity * NdotL;
    float3 lighting = diffuse + globals.ambient;
    float3 albedo = texColour.rgb * material.base_colour.rgb;

    return float4(albedo * lighting, texColour.a * material.base_colour.a);
}
