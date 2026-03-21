// Wayfinder Composition Fragment Shader
// Reads SceneColour texture and writes to swapchain (simple blit).
// Compiled with DXC: dxc -T ps_6_0 -E PSMain -spirv composition.frag -Fo composition.frag.spv
// SDL_GPU convention: fragment sampled textures at set 2

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
