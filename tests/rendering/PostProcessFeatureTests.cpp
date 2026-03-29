#include "app/EngineConfig.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/passes/ChromaticAberrationFeature.h"
#include "rendering/passes/ColourGradingFeature.h"
#include "rendering/passes/CompositionPass.h"
#include "rendering/passes/DebugPass.h"
#include "rendering/passes/SceneOpaquePass.h"
#include "rendering/passes/VignetteFeature.h"
#include "rendering/pipeline/RenderServices.h"
#include "volumes/BlendableEffectRegistry.h"

#include "core/Types.h"

#include <array>
#include <cstddef>
#include <string>

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
        feature.OnRegisterEffects(registry);
        const RenderFeatureContext ctx{services};
        feature.OnAttach(ctx);

        REQUIRE(registry.FindIdByName("chromatic_aberration").has_value());

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

        Wayfinder::Rendering::VignetteFeature feature;
        feature.OnRegisterEffects(registry);
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
        feature.OnRegisterEffects(registry);
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

    TEST_CASE("GetShaderPrograms returns expected descriptors for each feature")
    {
        SUBCASE("SceneOpaquePass returns 4 scene programs")
        {
            SceneOpaquePass pass;
            const auto programs = pass.GetShaderPrograms();
            REQUIRE(programs.size() == 4);
            CHECK(programs[0].Name == "unlit");
            CHECK(programs[1].Name == "unlit_blended");
            CHECK(programs[2].Name == "basic_lit");
            CHECK(programs[3].Name == "textured_lit");
        }

        SUBCASE("ChromaticAberrationFeature returns 1 program")
        {
            ChromaticAberrationFeature feature;
            const auto programs = feature.GetShaderPrograms();
            REQUIRE(programs.size() == 1);
            CHECK(programs[0].Name == "chromatic_aberration");
        }

        SUBCASE("VignetteFeature returns 1 program")
        {
            Rendering::VignetteFeature feature;
            const auto programs = feature.GetShaderPrograms();
            REQUIRE(programs.size() == 1);
            CHECK(programs[0].Name == "vignette");
        }

        SUBCASE("ColourGradingFeature returns 1 program")
        {
            ColourGradingFeature feature;
            const auto programs = feature.GetShaderPrograms();
            REQUIRE(programs.size() == 1);
            CHECK(programs[0].Name == "colour_grading");
        }

        SUBCASE("CompositionPass returns 1 program")
        {
            CompositionPass pass;
            const auto programs = pass.GetShaderPrograms();
            REQUIRE(programs.size() == 1);
            CHECK(programs[0].Name == "composition_blit");
        }

        SUBCASE("DebugPass returns 1 program with LineList primitive")
        {
            DebugPass pass;
            const auto programs = pass.GetShaderPrograms();
            REQUIRE(programs.size() == 1);
            CHECK(programs[0].Name == "debug_unlit");
            CHECK(programs[0].Primitive == PrimitiveType::LineList);
        }
    }

} // namespace Wayfinder::Tests
