#include "rendering/RenderDevice.h"

#include <doctest/doctest.h>

using namespace Wayfinder;

// ── BlendState Defaults ──────────────────────────────────

TEST_CASE("Default BlendState is disabled")
{
    BlendState state{};
    CHECK_FALSE(state.Enabled);
    CHECK(state.SrcColorFactor == BlendFactor::SrcAlpha);
    CHECK(state.DstColorFactor == BlendFactor::OneMinusSrcAlpha);
    CHECK(state.ColorOp == BlendOp::Add);
    CHECK(state.SrcAlphaFactor == BlendFactor::One);
    CHECK(state.DstAlphaFactor == BlendFactor::OneMinusSrcAlpha);
    CHECK(state.AlphaOp == BlendOp::Add);
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
    CHECK(state.SrcColorFactor == BlendFactor::SrcAlpha);
    CHECK(state.DstColorFactor == BlendFactor::OneMinusSrcAlpha);
    CHECK(state.ColorOp == BlendOp::Add);
    CHECK(state.SrcAlphaFactor == BlendFactor::One);
    CHECK(state.DstAlphaFactor == BlendFactor::OneMinusSrcAlpha);
    CHECK(state.AlphaOp == BlendOp::Add);
}

TEST_CASE("Additive preset uses One as dst factor")
{
    constexpr auto state = BlendPresets::Additive();
    CHECK(state.Enabled);
    CHECK(state.SrcColorFactor == BlendFactor::SrcAlpha);
    CHECK(state.DstColorFactor == BlendFactor::One);
    CHECK(state.ColorOp == BlendOp::Add);
    CHECK(state.SrcAlphaFactor == BlendFactor::SrcAlpha);
    CHECK(state.DstAlphaFactor == BlendFactor::One);
    CHECK(state.AlphaOp == BlendOp::Add);
}

TEST_CASE("Premultiplied preset uses One as src factor")
{
    constexpr auto state = BlendPresets::Premultiplied();
    CHECK(state.Enabled);
    CHECK(state.SrcColorFactor == BlendFactor::One);
    CHECK(state.DstColorFactor == BlendFactor::OneMinusSrcAlpha);
    CHECK(state.ColorOp == BlendOp::Add);
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
    CHECK(desc.blend.SrcColorFactor == BlendFactor::SrcAlpha);
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
