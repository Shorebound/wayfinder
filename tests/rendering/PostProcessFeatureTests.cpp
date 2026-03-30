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

    TEST_CASE("chromatic aberration registers as blendable effect on attach")
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

    TEST_CASE("vignette registers as blendable effect on attach")
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

    TEST_CASE("colour grading registers as blendable effect on attach")
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
        SUBCASE("SceneOpaquePass declares 4 programs with correct resource and depth settings")
        {
            SceneOpaquePass pass;
            const auto programs = pass.GetShaderPrograms();
            REQUIRE(programs.size() == 4);

            // unlit: no scene globals, depth on
            CHECK(programs[0].Name == "unlit");
            CHECK(programs[0].VertexShaderName == "unlit");
            CHECK(programs[0].FragmentShaderName == "unlit");
            CHECK(programs[0].NeedsSceneGlobals == false);
            CHECK(programs[0].DepthTest == true);
            CHECK(programs[0].DepthWrite == true);
            CHECK(programs[0].VertexLayout.AttributeCount == VertexLayouts::POSITION_NORMAL_COLOUR.AttributeCount);

            // unlit_blended: alpha blend, depth read-only
            CHECK(programs[1].Name == "unlit_blended");
            CHECK(programs[1].DepthTest == true);
            CHECK(programs[1].DepthWrite == false);

            // basic_lit: needs scene globals for lighting
            CHECK(programs[2].Name == "basic_lit");
            CHECK(programs[2].NeedsSceneGlobals == true);
            CHECK(programs[2].FragmentResources.UniformBuffers == 2);

            // textured_lit: needs scene globals + texture sampler
            CHECK(programs[3].Name == "textured_lit");
            CHECK(programs[3].NeedsSceneGlobals == true);
            CHECK(programs[3].FragmentResources.Samplers == 1);
            CHECK(programs[3].VertexLayout.AttributeCount == VertexLayouts::POSITION_NORMAL_UV_TANGENT.AttributeCount);
            CHECK(programs[3].TextureSlots.size() == 1);
        }

        SUBCASE("ChromaticAberrationFeature declares fullscreen pass with sampler input")
        {
            ChromaticAberrationFeature feature;
            const auto programs = feature.GetShaderPrograms();
            REQUIRE(programs.size() == 1);
            CHECK(programs[0].Name == "chromatic_aberration");
            CHECK(programs[0].VertexShaderName == "chromatic_aberration");
            CHECK(programs[0].FragmentResources.Samplers == 1);
            CHECK(programs[0].FragmentResources.UniformBuffers == 1);
            CHECK(programs[0].VertexLayout.AttributeCount == 0);
            CHECK(programs[0].NeedsSceneGlobals == false);
            CHECK(programs[0].DepthTest == false);
        }

        SUBCASE("VignetteFeature declares fullscreen pass with sampler input")
        {
            Rendering::VignetteFeature feature;
            const auto programs = feature.GetShaderPrograms();
            REQUIRE(programs.size() == 1);
            CHECK(programs[0].Name == "vignette");
            CHECK(programs[0].VertexShaderName == "vignette");
            CHECK(programs[0].FragmentResources.Samplers == 1);
            CHECK(programs[0].FragmentResources.UniformBuffers == 1);
            CHECK(programs[0].VertexLayout.AttributeCount == 0);
            CHECK(programs[0].NeedsSceneGlobals == false);
            CHECK(programs[0].DepthTest == false);
        }

        SUBCASE("ColourGradingFeature declares fullscreen pass with sampler input")
        {
            ColourGradingFeature feature;
            const auto programs = feature.GetShaderPrograms();
            REQUIRE(programs.size() == 1);
            CHECK(programs[0].Name == "colour_grading");
            CHECK(programs[0].VertexShaderName == "colour_grading");
            CHECK(programs[0].FragmentResources.Samplers == 1);
            CHECK(programs[0].FragmentResources.UniformBuffers == 1);
            CHECK(programs[0].VertexLayout.AttributeCount == 0);
            CHECK(programs[0].NeedsSceneGlobals == false);
            CHECK(programs[0].DepthTest == false);
        }

        SUBCASE("CompositionPass declares sampler-only blit with no UBO")
        {
            CompositionPass pass;
            const auto programs = pass.GetShaderPrograms();
            REQUIRE(programs.size() == 1);
            CHECK(programs[0].Name == "composition_blit");
            CHECK(programs[0].VertexShaderName == "fullscreen_copy");
            CHECK(programs[0].FragmentShaderName == "fullscreen_copy");
            CHECK(programs[0].FragmentResources.Samplers == 1);
            CHECK(programs[0].FragmentResources.UniformBuffers == 0);
            CHECK(programs[0].VertexLayout.AttributeCount == 0);
            CHECK(programs[0].NeedsSceneGlobals == false);
            CHECK(programs[0].MaterialUBOSize == 0);
        }

        SUBCASE("DebugPass declares line and solid geometry programs")
        {
            DebugPass pass;
            const auto programs = pass.GetShaderPrograms();
            REQUIRE(programs.size() == 2);

            // Lines: PosColour, no depth, no culling
            CHECK(programs[0].Name == "debug_unlit");
            CHECK(programs[0].VertexLayout.AttributeCount == VertexLayouts::POSITION_COLOUR.AttributeCount);
            CHECK(programs[0].Cull == CullMode::None);
            CHECK(programs[0].DepthTest == false);

            // Solid boxes: PosNormalColour, back-face culled
            CHECK(programs[1].Name == "debug_solid");
            CHECK(programs[1].VertexShaderName == "unlit");
            CHECK(programs[1].VertexLayout.AttributeCount == VertexLayouts::POSITION_NORMAL_COLOUR.AttributeCount);
            CHECK(programs[1].Cull == CullMode::Back);
        }
    }

} // namespace Wayfinder::Tests
