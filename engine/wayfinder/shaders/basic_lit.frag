// Wayfinder Basic Lit Fragment Shader
// Compiled with DXC: dxc -T ps_6_0 -E PSMain -spirv basic_lit.frag -Fo basic_lit.frag.spv
// SDL_GPU SPIR-V convention: fragment UBOs at set 3

// Material parameters (binding 0) — serialized from MaterialParameterBlock
[[vk::binding(0, 3)]]
cbuffer MaterialUBO : register(b0)
{
    float4 base_color;
};

// Per-frame scene globals (binding 1) — pushed once per frame by the renderer
[[vk::binding(1, 3)]]
cbuffer SceneGlobalsUBO : register(b1)
{
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
