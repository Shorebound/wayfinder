#include <doctest/doctest.h>

#include "rendering/materials/RenderingEffects.h"
#include "volumes/BlendableEffect.h"
#include "volumes/BlendableEffectRegistry.h"
#include "volumes/Override.h"
#include "volumes/OverrideReflection.h"

#include <array>
#include <nlohmann/json.hpp>
#include <tuple>

namespace
{
    struct RegistryTestEffectParams
    {
        Wayfinder::Override<float> Amount{0.0f};

        static constexpr auto FIELDS = std::make_tuple(Wayfinder::FieldDesc{&RegistryTestEffectParams::Amount, std::string_view{"amount"}});
    };

} // namespace

TEST_CASE("BlendableEffectRegistry registers and finds by name")
{
    Wayfinder::BlendableEffectRegistry registry;
    const Wayfinder::BlendableEffectId id = registry.Register<RegistryTestEffectParams>("test_effect");
    CHECK(id != Wayfinder::INVALID_BLENDABLE_EFFECT_ID);
    registry.Seal();

    const Wayfinder::BlendableEffectDesc* desc = registry.Find(id);
    REQUIRE(desc != nullptr);
    CHECK(desc->Name == "test_effect");

    const auto byName = registry.FindIdByName("test_effect");
    REQUIRE(byName.has_value());
    CHECK(*byName == id);
}

TEST_CASE("LerpOverride leaves inactive fields unchanged")
{
    Wayfinder::ColourGradingParams current{};
    Wayfinder::ColourGradingParams source{};
    source.ExposureStops = Wayfinder::Override<float>::Set(2.0f);

    const Wayfinder::ColourGradingParams blended = Wayfinder::BlendableEffectTraits<Wayfinder::ColourGradingParams>::Lerp(current, source, 0.5f);
    CHECK(blended.ExposureStops.Active);
    CHECK(blended.ExposureStops.Value == doctest::Approx(1.0f));
    CHECK(!blended.Contrast.Active);
    CHECK(blended.Contrast.Value == doctest::Approx(1.0f));
}

TEST_CASE("BlendableEffectRegistry blend and JSON round-trip for test effect")
{
    Wayfinder::BlendableEffectRegistry registry;
    const Wayfinder::BlendableEffectId id = registry.Register<RegistryTestEffectParams>("test_effect");
    registry.Seal();

    const Wayfinder::BlendableEffectDesc* desc = registry.Find(id);
    REQUIRE(desc != nullptr);

    alignas(16) std::array<std::byte, Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY> dst{};
    alignas(16) std::array<std::byte, Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY> src{};

    desc->CreateIdentity(dst.data());
    desc->CreateIdentity(src.data());
    nlohmann::json j;
    j["amount"] = 4.0f;
    desc->Deserialise(src.data(), j);

    desc->Blend(dst.data(), src.data(), 0.5f);

    const auto* result = std::launder(reinterpret_cast<const RegistryTestEffectParams*>(dst.data()));
    REQUIRE(result != nullptr);
    CHECK(result->Amount.Active);
    CHECK(result->Amount.Value == doctest::Approx(2.0f));

    nlohmann::json out;
    desc->Serialise(out, dst.data());
    CHECK(out["amount"].get<float>() == doctest::Approx(2.0f));

    alignas(16) std::array<std::byte, Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY> roundTrip{};
    desc->CreateIdentity(roundTrip.data());
    desc->Deserialise(roundTrip.data(), out);
    const auto* again = std::launder(reinterpret_cast<const RegistryTestEffectParams*>(roundTrip.data()));
    REQUIRE(again != nullptr);
    CHECK(again->Amount.Value == doctest::Approx(2.0f));

    desc->Destroy(dst.data());
    desc->Destroy(src.data());
    desc->Destroy(roundTrip.data());
}

TEST_CASE("BlendableEffectRegistry Deserialise overwrites buffer after CreateIdentity")
{
    Wayfinder::BlendableEffectRegistry registry;
    const Wayfinder::BlendableEffectId id = registry.Register<RegistryTestEffectParams>("test_effect");
    registry.Seal();

    const Wayfinder::BlendableEffectDesc* desc = registry.Find(id);
    REQUIRE(desc != nullptr);

    alignas(16) std::array<std::byte, Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY> buf{};
    desc->CreateIdentity(buf.data());

    nlohmann::json j;
    j["amount"] = 4.0f;
    desc->Deserialise(buf.data(), j);

    const auto* params = std::launder(reinterpret_cast<const RegistryTestEffectParams*>(buf.data()));
    REQUIRE(params != nullptr);
    CHECK(params->Amount.Active);
    CHECK(params->Amount.Value == doctest::Approx(4.0f));

    desc->Destroy(buf.data());
}
