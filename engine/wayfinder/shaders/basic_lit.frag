// Wayfinder Basic Lit Fragment Shader
// Compiled with DXC: dxc -T ps_6_0 -E PSMain -spirv basic_lit.frag -Fo basic_lit.frag.spv
// SDL_GPU SPIR-V convention: fragment UBO at set 3

[[vk::binding(0, 3)]]
cbuffer MaterialLightUBO : register(b0)
{
    float4 base_color;
    float3 light_direction;
    float  light_intensity;
    float3 light_color;
    float  ambient;
};

struct PSInput
{
    float4 Position : SV_Position;
    float3 Normal   : TEXCOORD0;
    float3 Color    : TEXCOORD1;
};

float4 PSMain(PSInput input) : SV_Target
{
    float3 N = normalize(input.Normal);
    float3 L = normalize(-light_direction);
    float NdotL = max(dot(N, L), 0.0);

    float3 diffuse = light_color * light_intensity * NdotL;
    float3 lighting = diffuse + ambient;
    float3 albedo = input.Color * base_color.rgb;

    return float4(albedo * lighting, base_color.a);
}
