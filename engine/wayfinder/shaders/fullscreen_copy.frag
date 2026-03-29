// Passthrough fullscreen copy: sample scene colour, no UBO.
// Compiled with DXC: dxc -T ps_6_0 -E PSMain -spirv fullscreen_copy.frag -Fo fullscreen_copy.frag.spv
// SDL_GPU convention: fragment samplers at set 2 (matches other fullscreen passes)

[[vk::combinedImageSampler]]
[[vk::binding(0, 2)]]
Texture2D<float4> SceneColourTex : register(t0, space2);

[[vk::combinedImageSampler]]
[[vk::binding(0, 2)]]
SamplerState PointSampler : register(s0, space2);

struct PSInput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_Target
{
    return SceneColourTex.Sample(PointSampler, input.TexCoord);
}
