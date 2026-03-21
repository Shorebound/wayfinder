#include "rendering/backend/RenderDevice.h"
#include "rendering/pipeline/PipelineCache.h"

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

TEST_CASE("Multiplicative preset uses DstColour as src factor")
{
    constexpr auto state = BlendPresets::Multiplicative();
    CHECK(state.Enabled);
    CHECK(state.SrcColourFactor == BlendFactor::DstColour);
    CHECK(state.DstColourFactor == BlendFactor::Zero);
    CHECK(state.ColourOp == BlendOp::Add);
    CHECK(state.SrcAlphaFactor == BlendFactor::DstAlpha);
    CHECK(state.DstAlphaFactor == BlendFactor::Zero);
    CHECK(state.AlphaOp == BlendOp::Add);
}

// ── PipelineCreateDesc includes blend ────────────────────

TEST_CASE("PipelineCreateDesc default blend is disabled")
{
    PipelineCreateDesc desc{};
    CHECK(desc.numColourTargets == 1);
    CHECK_FALSE(desc.colourTargetBlends[0].Enabled);
}

TEST_CASE("PipelineCreateDesc carries blend state")
{
    PipelineCreateDesc desc{};
    desc.colourTargetBlends[0] = BlendPresets::AlphaBlend();
    CHECK(desc.colourTargetBlends[0].Enabled);
    CHECK(desc.colourTargetBlends[0].SrcColourFactor == BlendFactor::SrcAlpha);
}

// ── Presets are constexpr ────────────────────────────────

TEST_CASE("BlendPresets are usable in constexpr context")
{
    static constexpr auto opaque = BlendPresets::Opaque();
    static constexpr auto alpha = BlendPresets::AlphaBlend();
    static constexpr auto additive = BlendPresets::Additive();
    static constexpr auto premul = BlendPresets::Premultiplied();
    static constexpr auto multiply = BlendPresets::Multiplicative();

    CHECK_FALSE(opaque.Enabled);
    CHECK(alpha.Enabled);
    CHECK(additive.Enabled);
    CHECK(premul.Enabled);
    CHECK(multiply.Enabled);
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

// ── operator== ───────────────────────────────────────────

TEST_CASE("BlendState operator== detects identical states")
{
    CHECK(BlendPresets::AlphaBlend() == BlendPresets::AlphaBlend());
    CHECK(BlendPresets::Opaque() == BlendPresets::Opaque());
    CHECK(BlendPresets::Additive() == BlendPresets::Additive());
    CHECK(BlendPresets::Premultiplied() == BlendPresets::Premultiplied());
    CHECK(BlendPresets::Multiplicative() == BlendPresets::Multiplicative());
}

TEST_CASE("BlendState operator== detects different states")
{
    CHECK_FALSE(BlendPresets::Opaque() == BlendPresets::AlphaBlend());
    CHECK_FALSE(BlendPresets::AlphaBlend() == BlendPresets::Additive());
    CHECK_FALSE(BlendPresets::Additive() == BlendPresets::Premultiplied());
    CHECK_FALSE(BlendPresets::Premultiplied() == BlendPresets::Multiplicative());
}

TEST_CASE("BlendState operator== is sensitive to ColourWriteMask")
{
    BlendState a = BlendPresets::AlphaBlend();
    BlendState b = BlendPresets::AlphaBlend();
    CHECK(a == b);

    b.ColourWriteMask = 0x7;
    CHECK_FALSE(a == b);
}

// ── New blend factors exist ──────────────────────────────

TEST_CASE("DstColour and OneMinusDstColour factors are available")
{
    BlendState state{};
    state.SrcColourFactor = BlendFactor::DstColour;
    CHECK(state.SrcColourFactor == BlendFactor::DstColour);

    state.DstColourFactor = BlendFactor::OneMinusDstColour;
    CHECK(state.DstColourFactor == BlendFactor::OneMinusDstColour);
}

TEST_CASE("ConstantColour and OneMinusConstantColour factors are available")
{
    BlendState state{};
    state.SrcColourFactor = BlendFactor::ConstantColour;
    CHECK(state.SrcColourFactor == BlendFactor::ConstantColour);

    state.DstColourFactor = BlendFactor::OneMinusConstantColour;
    CHECK(state.DstColourFactor == BlendFactor::OneMinusConstantColour);
}

// ── Multiple colour targets ──────────────────────────────

TEST_CASE("PipelineCreateDesc supports multiple colour targets")
{
    PipelineCreateDesc desc{};
    desc.numColourTargets = 3;
    desc.colourTargetBlends[0] = BlendPresets::AlphaBlend();
    desc.colourTargetBlends[1] = BlendPresets::Opaque();
    desc.colourTargetBlends[2] = BlendPresets::Additive();

    CHECK(desc.numColourTargets == 3);
    CHECK(desc.colourTargetBlends[0].Enabled);
    CHECK_FALSE(desc.colourTargetBlends[1].Enabled);
    CHECK(desc.colourTargetBlends[2].DstColourFactor == BlendFactor::One);
}

TEST_CASE("MAX_COLOUR_TARGETS is 8")
{
    CHECK(MAX_COLOUR_TARGETS == 8);
}

// ── Pipeline cache hash — blend differentiation ─────────

TEST_CASE("Pipeline hash differs when blend preset changes")
{
    PipelineCreateDesc opaqueDesc{};
    opaqueDesc.colourTargetBlends[0] = BlendPresets::Opaque();

    PipelineCreateDesc alphaDesc{};
    alphaDesc.colourTargetBlends[0] = BlendPresets::AlphaBlend();

    CHECK(PipelineCache::HashDesc(opaqueDesc) != PipelineCache::HashDesc(alphaDesc));
}

TEST_CASE("Pipeline hash differs when only ColourWriteMask changes")
{
    PipelineCreateDesc fullMaskDesc{};
    fullMaskDesc.colourTargetBlends[0] = BlendPresets::AlphaBlend();

    PipelineCreateDesc rgbOnlyDesc{};
    rgbOnlyDesc.colourTargetBlends[0] = BlendPresets::AlphaBlend();
    rgbOnlyDesc.colourTargetBlends[0].ColourWriteMask = 0x7; // RGB only

    CHECK(PipelineCache::HashDesc(fullMaskDesc) != PipelineCache::HashDesc(rgbOnlyDesc));
}

TEST_CASE("Pipeline hash is equal for identical blend states")
{
    PipelineCreateDesc firstDesc{};
    firstDesc.colourTargetBlends[0] = BlendPresets::AlphaBlend();

    PipelineCreateDesc secondDesc{};
    secondDesc.colourTargetBlends[0] = BlendPresets::AlphaBlend();

    CHECK(PipelineCache::HashDesc(firstDesc) == PipelineCache::HashDesc(secondDesc));
}

TEST_CASE("Pipeline hash differs when numColourTargets differs")
{
    PipelineCreateDesc singleTarget{};
    singleTarget.numColourTargets = 1;
    singleTarget.colourTargetBlends[0] = BlendPresets::Opaque();

    PipelineCreateDesc dualTarget{};
    dualTarget.numColourTargets = 2;
    dualTarget.colourTargetBlends[0] = BlendPresets::Opaque();
    dualTarget.colourTargetBlends[1] = BlendPresets::Opaque();

    CHECK(PipelineCache::HashDesc(singleTarget) != PipelineCache::HashDesc(dualTarget));
}

TEST_CASE("Pipeline hash differs when second target blend changes")
{
    PipelineCreateDesc descA{};
    descA.numColourTargets = 2;
    descA.colourTargetBlends[0] = BlendPresets::Opaque();
    descA.colourTargetBlends[1] = BlendPresets::Opaque();

    PipelineCreateDesc descB{};
    descB.numColourTargets = 2;
    descB.colourTargetBlends[0] = BlendPresets::Opaque();
    descB.colourTargetBlends[1] = BlendPresets::AlphaBlend();

    CHECK(PipelineCache::HashDesc(descA) != PipelineCache::HashDesc(descB));
}
