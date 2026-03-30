#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/pipeline/PipelineCache.h"

#include <doctest/doctest.h>

using namespace Wayfinder;

// ── BlendState Defaults ──────────────────────────────────
namespace Wayfinder::Tests
{
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
        CHECK(desc.ColourTargetCount == 1);
        CHECK_FALSE(desc.ColourTargetBlends[0].Enabled);
    }

    TEST_CASE("PipelineCreateDesc carries blend state")
    {
        PipelineCreateDesc desc{};
        desc.ColourTargetBlends[0] = BlendPresets::AlphaBlend();
        CHECK(desc.ColourTargetBlends[0].Enabled);
        CHECK(desc.ColourTargetBlends[0].SrcColourFactor == BlendFactor::SrcAlpha);
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
        desc.ColourTargetCount = 3;
        desc.ColourTargetBlends[0] = BlendPresets::AlphaBlend();
        desc.ColourTargetBlends[1] = BlendPresets::Opaque();
        desc.ColourTargetBlends[2] = BlendPresets::Additive();

        CHECK(desc.ColourTargetCount == 3);
        CHECK(desc.ColourTargetBlends[0].Enabled);
        CHECK_FALSE(desc.ColourTargetBlends[1].Enabled);
        CHECK(desc.ColourTargetBlends[2].DstColourFactor == BlendFactor::One);
    }

    TEST_CASE("MAX_COLOUR_TARGETS is 8")
    {
        CHECK(MAX_COLOUR_TARGETS == 8);
    }

    // ── Pipeline cache hash — blend differentiation ─────────

    TEST_CASE("Pipeline hash differs when blend preset changes")
    {
        PipelineCreateDesc opaqueDesc{};
        opaqueDesc.ColourTargetBlends[0] = BlendPresets::Opaque();

        PipelineCreateDesc alphaDesc{};
        alphaDesc.ColourTargetBlends[0] = BlendPresets::AlphaBlend();

        CHECK(PipelineCache::HashDesc(opaqueDesc) != PipelineCache::HashDesc(alphaDesc));
    }

    TEST_CASE("Pipeline hash differs when only ColourWriteMask changes")
    {
        PipelineCreateDesc fullMaskDesc{};
        fullMaskDesc.ColourTargetBlends[0] = BlendPresets::AlphaBlend();

        PipelineCreateDesc rgbOnlyDesc{};
        rgbOnlyDesc.ColourTargetBlends[0] = BlendPresets::AlphaBlend();
        rgbOnlyDesc.ColourTargetBlends[0].ColourWriteMask = 0x7; // RGB only

        CHECK(PipelineCache::HashDesc(fullMaskDesc) != PipelineCache::HashDesc(rgbOnlyDesc));
    }

    TEST_CASE("Pipeline hash is equal for identical blend states")
    {
        PipelineCreateDesc firstDesc{};
        firstDesc.ColourTargetBlends[0] = BlendPresets::AlphaBlend();

        PipelineCreateDesc secondDesc{};
        secondDesc.ColourTargetBlends[0] = BlendPresets::AlphaBlend();

        CHECK(PipelineCache::HashDesc(firstDesc) == PipelineCache::HashDesc(secondDesc));
    }

    TEST_CASE("Pipeline hash differs when numColourTargets differs")
    {
        PipelineCreateDesc singleTarget{};
        singleTarget.ColourTargetCount = 1;
        singleTarget.ColourTargetBlends[0] = BlendPresets::Opaque();

        PipelineCreateDesc dualTarget{};
        dualTarget.ColourTargetCount = 2;
        dualTarget.ColourTargetBlends[0] = BlendPresets::Opaque();
        dualTarget.ColourTargetBlends[1] = BlendPresets::Opaque();

        CHECK(PipelineCache::HashDesc(singleTarget) != PipelineCache::HashDesc(dualTarget));
    }

    TEST_CASE("Pipeline hash differs when second target blend changes")
    {
        PipelineCreateDesc descA{};
        descA.ColourTargetCount = 2;
        descA.ColourTargetBlends[0] = BlendPresets::Opaque();
        descA.ColourTargetBlends[1] = BlendPresets::Opaque();

        PipelineCreateDesc descB{};
        descB.ColourTargetCount = 2;
        descB.ColourTargetBlends[0] = BlendPresets::Opaque();
        descB.ColourTargetBlends[1] = BlendPresets::AlphaBlend();

        CHECK(PipelineCache::HashDesc(descA) != PipelineCache::HashDesc(descB));
    }

    // ── VertexLayoutsMatch ───────────────────────────────────

    TEST_CASE("VertexLayoutsMatch returns true for identical layouts")
    {
        CHECK(VertexLayoutsMatch(VertexLayouts::POSITION_COLOUR, VertexLayouts::POSITION_COLOUR));
        CHECK(VertexLayoutsMatch(VertexLayouts::POSITION_NORMAL_UV, VertexLayouts::POSITION_NORMAL_UV));
        CHECK(VertexLayoutsMatch(VertexLayouts::EMPTY, VertexLayouts::EMPTY));
    }

    TEST_CASE("VertexLayoutsMatch returns false for mismatched stride")
    {
        VertexLayout a = VertexLayouts::POSITION;
        VertexLayout b = VertexLayouts::POSITION;
        b.Stride = a.Stride + 4;
        CHECK_FALSE(VertexLayoutsMatch(a, b));
    }

    TEST_CASE("VertexLayoutsMatch returns false for mismatched attribute count")
    {
        CHECK_FALSE(VertexLayoutsMatch(VertexLayouts::POSITION, VertexLayouts::POSITION_COLOUR));
    }

    TEST_CASE("VertexLayoutsMatch returns false for per-attribute differences")
    {
        std::array<VertexAttribute, 2> attribsA = {{
            {0, 0, VertexAttributeFormat::Float3},
            {1, 12, VertexAttributeFormat::Float3},
        }};
        std::array<VertexAttribute, 2> attribsB = {{
            {0, 0, VertexAttributeFormat::Float3},
            {1, 12, VertexAttributeFormat::Float2}, // different format
        }};

        VertexLayout a{.Stride = 24, .Attributes = attribsA.data(), .AttributeCount = 2};
        VertexLayout b{.Stride = 24, .Attributes = attribsB.data(), .AttributeCount = 2};
        CHECK_FALSE(VertexLayoutsMatch(a, b));
    }

    TEST_CASE("VertexLayoutsMatch returns false when attributes pointer is null but count > 0")
    {
        VertexLayout a{.Stride = 12, .Attributes = VertexLayouts::POSITION_ATTRIBUTES.data(), .AttributeCount = 1};
        VertexLayout b{.Stride = 12, .Attributes = nullptr, .AttributeCount = 1};
        CHECK_FALSE(VertexLayoutsMatch(a, b));
        CHECK_FALSE(VertexLayoutsMatch(b, a));
    }
}