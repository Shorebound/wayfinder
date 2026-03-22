// Wayfinder Textured Lit Fragment Shader
// Samples a diffuse texture and applies basic directional lighting.
// Compiled with DXC: dxc -T ps_6_0 -E PSMain -spirv textured_lit.frag -Fo textured_lit.frag.spv
// SDL_GPU convention: fragment sampled textures at set 2, fragment UBOs at set 3

// Diffuse texture (binding 0, set 2)
[[vk::combinedImageSampler]]
[[vk::binding(0, 2)]]
Texture2D<float4> DiffuseTex : register(t0, space2);

[[vk::combinedImageSampler]]
[[vk::binding(0, 2)]]
SamplerState DiffuseSampler : register(s0, space2);

// Material parameters (binding 0) — serialised from MaterialParameterBlock
[[vk::binding(0, 3)]]
cbuffer MaterialUBO : register(b0)
{
    float4 base_colour;
};

// Per-frame scene globals (binding 1) — pushed once per frame by the renderer
[[vk::binding(1, 3)]]
cbuffer SceneGlobalsUBO : register(b1)
{
    float3 light_direction;
    float  light_intensity;
    float3 light_colour;
    float  ambient;
};

struct PSInput
{
    float4 Position : SV_Position;
    float3 Normal   : TEXCOORD0;
    float2 UV       : TEXCOORD1;
};

float4 PSMain(PSInput input) : SV_Target
{
    float4 texColour = DiffuseTex.Sample(DiffuseSampler, input.UV);

    float3 N = normalize(input.Normal);
    float3 L = normalize(-light_direction);
    float NdotL = max(dot(N, L), 0.0);

    float3 diffuse = light_colour * light_intensity * NdotL;
    float3 lighting = diffuse + ambient;
    float3 albedo = texColour.rgb * base_colour.rgb;

    return float4(albedo * lighting, texColour.a * base_colour.a);
}
