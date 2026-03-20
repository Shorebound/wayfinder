#include "rendering/RenderDevice.h"
#include "rendering/PipelineCache.h"

#include <doctest/doctest.h>

using namespace Wayfinder;

// ── BlendState Defaults ──────────────────────────────────

TEST_CASE("Default BlendState is disabled")
{
    BlendState state{};
    CHECK_FALSE(state.Enabled);
    CHECK(state.SrcColourFactor == BlendFactor::SrcAlpha);
    CHECK(state.DstColourFactor == BlendFactor::OneMinusSrcAlpha);
    CHECK(state.ColourOp == BlendOp::Add);
    CHECK(state.SrcAlphaFactor == BlendFactor::One);
    CHECK(state.DstAlphaFactor == BlendFactor::OneMinusSrcAlpha);
    CHECK(state.AlphaOp == BlendOp::Add);
    CHECK(state.ColourWriteMask == 0xF);
}

// ── BlendPresets ─────────────────────────────────────────

TEST_CASE("Opaque preset is disabled")
{
    constexpr auto state = BlendPresets::Opaque();
    CHECK_FALSE(state.Enabled);
}

TEST_CASE("AlphaBlend preset has correct factors")
{
    constexpr auto state = BlendPresets::AlphaBlend();
    CHECK(state.Enabled);
    CHECK(state.SrcColourFactor == BlendFactor::SrcAlpha);
    CHECK(state.DstColourFactor == BlendFactor::OneMinusSrcAlpha);
    CHECK(state.ColourOp == BlendOp::Add);
    CHECK(state.SrcAlphaFactor == BlendFactor::One);
    CHECK(state.DstAlphaFactor == BlendFactor::OneMinusSrcAlpha);
    CHECK(state.AlphaOp == BlendOp::Add);
}

TEST_CASE("Additive preset uses One as dst factor")
{
    constexpr auto state = BlendPresets::Additive();
    CHECK(state.Enabled);
    CHECK(state.SrcColourFactor == BlendFactor::SrcAlpha);
    CHECK(state.DstColourFactor == BlendFactor::One);
    CHECK(state.ColourOp == BlendOp::Add);
    CHECK(state.SrcAlphaFactor == BlendFactor::SrcAlpha);
    CHECK(state.DstAlphaFactor == BlendFactor::One);
    CHECK(state.AlphaOp == BlendOp::Add);
}

TEST_CASE("Premultiplied preset uses One as src factor")
{
    constexpr auto state = BlendPresets::Premultiplied();
    CHECK(state.Enabled);
    CHECK(state.SrcColourFactor == BlendFactor::One);
    CHECK(state.DstColourFactor == BlendFactor::OneMinusSrcAlpha);
    CHECK(state.ColourOp == BlendOp::Add);
    CHECK(state.SrcAlphaFactor == BlendFactor::One);
    CHECK(state.DstAlphaFactor == BlendFactor::OneMinusSrcAlpha);
    CHECK(state.AlphaOp == BlendOp::Add);
}

// ── PipelineCreateDesc includes blend ────────────────────

TEST_CASE("PipelineCreateDesc default blend is disabled")
{
    PipelineCreateDesc desc{};
    CHECK_FALSE(desc.blend.Enabled);
}

TEST_CASE("PipelineCreateDesc carries blend state")
{
    PipelineCreateDesc desc{};
    desc.blend = BlendPresets::AlphaBlend();
    CHECK(desc.blend.Enabled);
    CHECK(desc.blend.SrcColourFactor == BlendFactor::SrcAlpha);
}

// ── Presets are constexpr ────────────────────────────────

TEST_CASE("BlendPresets are usable in constexpr context")
{
    static constexpr auto opaque = BlendPresets::Opaque();
    static constexpr auto alpha = BlendPresets::AlphaBlend();
    static constexpr auto additive = BlendPresets::Additive();
    static constexpr auto premul = BlendPresets::Premultiplied();

    CHECK_FALSE(opaque.Enabled);
    CHECK(alpha.Enabled);
    CHECK(additive.Enabled);
    CHECK(premul.Enabled);
}

// ── ColourWriteMask ──────────────────────────────────────

TEST_CASE("ColourWriteMask defaults to all channels")
{
    constexpr auto alpha = BlendPresets::AlphaBlend();
    CHECK(alpha.ColourWriteMask == 0xF);
}

TEST_CASE("ColourWriteMask can be customised")
{
    BlendState state = BlendPresets::AlphaBlend();
    state.ColourWriteMask = 0x7; // RGB only
    CHECK(state.ColourWriteMask == 0x7);
}

// ── Pipeline cache hash — blend differentiation ─────────

TEST_CASE("Pipeline hash differs when blend preset changes")
{
    PipelineCreateDesc opaqueDesc{};
    opaqueDesc.blend = BlendPresets::Opaque();

    PipelineCreateDesc alphaDesc{};
    alphaDesc.blend = BlendPresets::AlphaBlend();

    CHECK(PipelineCache::HashDesc(opaqueDesc) != PipelineCache::HashDesc(alphaDesc));
}

TEST_CASE("Pipeline hash differs when only ColourWriteMask changes")
{
    PipelineCreateDesc fullMaskDesc{};
    fullMaskDesc.blend = BlendPresets::AlphaBlend();

    PipelineCreateDesc rgbOnlyDesc{};
    rgbOnlyDesc.blend = BlendPresets::AlphaBlend();
    rgbOnlyDesc.blend.ColourWriteMask = 0x7; // RGB only

    CHECK(PipelineCache::HashDesc(fullMaskDesc) != PipelineCache::HashDesc(rgbOnlyDesc));
}

TEST_CASE("Pipeline hash is equal for identical blend states")
{
    PipelineCreateDesc firstDesc{};
    firstDesc.blend = BlendPresets::AlphaBlend();

    PipelineCreateDesc secondDesc{};
    secondDesc.blend = BlendPresets::AlphaBlend();

    CHECK(PipelineCache::HashDesc(firstDesc) == PipelineCache::HashDesc(secondDesc));
}
