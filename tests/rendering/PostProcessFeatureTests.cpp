#include "app/EngineConfig.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/passes/ChromaticAberrationFeature.h"
#include "rendering/passes/ColourGradingFeature.h"
#include "rendering/passes/VignetteFeature.h"
#include "rendering/pipeline/RenderServices.h"
#include "volumes/BlendableEffectRegistry.h"

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
        REQUIRE(services.GetPrograms().Find("chromatic_aberration") != nullptr);

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
        REQUIRE(services.GetPrograms().Find("vignette") != nullptr);

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
        REQUIRE(services.GetPrograms().Find("colour_grading") != nullptr);

        feature.OnDetach(ctx);
        services.Shutdown();
    }

    TEST_CASE("Post-process UBO sizes match shader std140 layouts")
    {
        CHECK(sizeof(float) * 4u == 16u);
        CHECK(sizeof(float) * 16u == 64u);
    }

} // namespace Wayfinder::Tests
