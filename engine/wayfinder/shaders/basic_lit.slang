import "modules/transforms";
import "modules/scene_globals";

[[vk::binding(0, 1)]]
ConstantBuffer<TransformData> transform;

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
    float3 Colour : TEXCOORD1;
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
    output.Normal = normalize(mul((float3x3)transform.model, input.Normal));
    output.Colour = input.Colour;
    return output;
}

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_Target
{
    float3 N = normalize(input.Normal);
    float3 L = normalize(-globals.light_direction);
    float NdotL = max(dot(N, L), 0.0);

    float3 diffuse = globals.light_colour * globals.light_intensity * NdotL;
    float3 lighting = diffuse + globals.ambient;
    float3 albedo = input.Colour * material.base_colour.rgb;

    return float4(albedo * lighting, material.base_colour.a);
}
