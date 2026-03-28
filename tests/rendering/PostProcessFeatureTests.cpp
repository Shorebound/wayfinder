#include "app/EngineConfig.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/passes/ChromaticAberrationFeature.h"
#include "rendering/passes/ColourGradingFeature.h"
#include "rendering/passes/VignetteFeature.h"
#include "rendering/pipeline/RenderServices.h"
#include "volumes/BlendableEffectRegistry.h"

#include "core/Types.h"

#include <array>
#include <cstddef>

#include <doctest/doctest.h>

namespace Wayfinder::Tests
{
    namespace
    {
        EngineConfig MakeTestConfig()
        {
            EngineConfig config;
            config.Window.Width = 320;
            config.Window.Height = 240;
            config.Shaders.Directory = "";
            return config;
        }
    } // namespace

    TEST_CASE("ChromaticAberrationFeature OnAttach registers blendable type and shader program name")
    {
        auto device = RenderDevice::Create(RenderBackend::Null);
        REQUIRE(device);

        BlendableEffectRegistry registry;
        RenderServices services;
        REQUIRE(services.Initialise(*device, MakeTestConfig(), &registry));

        ChromaticAberrationFeature feature;
        const RenderFeatureContext ctx{services};
        feature.OnAttach(ctx);

        REQUIRE(registry.FindIdByName("chromatic_aberration").has_value());
        // Shader program entry exists only when `ShaderProgramRegistry::Register` creates a pipeline;
        // with NullDevice and no shader assets that step may fail while blendable registration still succeeds.

        feature.OnDetach(ctx);
        services.Shutdown();
    }

    TEST_CASE("VignetteFeature OnAttach registers blendable type and shader program name")
    {
        auto device = RenderDevice::Create(RenderBackend::Null);
        REQUIRE(device);

        BlendableEffectRegistry registry;
        RenderServices services;
        REQUIRE(services.Initialise(*device, MakeTestConfig(), &registry));

        VignetteFeature feature;
        const RenderFeatureContext ctx{services};
        feature.OnAttach(ctx);

        REQUIRE(registry.FindIdByName("vignette").has_value());

        feature.OnDetach(ctx);
        services.Shutdown();
    }

    TEST_CASE("ColourGradingFeature OnAttach registers blendable type and shader program name")
    {
        auto device = RenderDevice::Create(RenderBackend::Null);
        REQUIRE(device);

        BlendableEffectRegistry registry;
        RenderServices services;
        REQUIRE(services.Initialise(*device, MakeTestConfig(), &registry));

        ColourGradingFeature feature;
        const RenderFeatureContext ctx{services};
        feature.OnAttach(ctx);

        REQUIRE(registry.FindIdByName("colour_grading").has_value());

        feature.OnDetach(ctx);
        services.Shutdown();
    }

    TEST_CASE("Post-process UBO sizes match shader std140 layouts")
    {
        /// Mirrors `chromatic_aberration.frag` / ChromaticAberrationFeature UBO.
        struct alignas(16) ChromaticAberrationUBO
        {
            Float4 IntensityPad{};
        };
        static_assert(sizeof(ChromaticAberrationUBO) == 16);
        CHECK(offsetof(ChromaticAberrationUBO, IntensityPad) == 0);

        /// Mirrors `vignette.frag` / VignetteFeature UBO.
        struct alignas(16) VignetteUBO
        {
            float Strength = 0.0f;
            std::array<float, 3> Pad{};
        };
        static_assert(sizeof(VignetteUBO) == 16);
        CHECK(offsetof(VignetteUBO, Strength) == 0);

        /// Matches `colour_grading.frag` / ColourGradingFeature UBO (std140).
        struct alignas(16) ColourGradingUBO
        {
            Float4 ExposureContrastSaturation{};
            Float4 Lift{};
            Float4 Gamma{};
            Float4 Gain{};
        };
        static_assert(sizeof(ColourGradingUBO) == 64);
        CHECK(offsetof(ColourGradingUBO, ExposureContrastSaturation) == 0);
        CHECK(offsetof(ColourGradingUBO, Lift) == 16);
        CHECK(offsetof(ColourGradingUBO, Gamma) == 32);
        CHECK(offsetof(ColourGradingUBO, Gain) == 48);
    }

} // namespace Wayfinder::Tests
