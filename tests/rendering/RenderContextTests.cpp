#include "rendering/pipeline/RenderContext.h"
#include "rendering/backend/RenderDevice.h"
#include "core/EngineConfig.h"

#include <doctest/doctest.h>

namespace
{
    Wayfinder::EngineConfig MakeTestConfig()
    {
        Wayfinder::EngineConfig config;
        config.Window.Width = 320;
        config.Window.Height = 240;
        config.Shaders.Directory = ""; // NullDevice doesn't load files
        return config;
    }
}

TEST_CASE("RenderContext initializes and shuts down with NullDevice")
{
    auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
    REQUIRE(device);

    Wayfinder::RenderContext context;
    CHECK(context.Initialize(*device, MakeTestConfig()));
    context.Shutdown();
}

TEST_CASE("RenderContext getters return valid references after init")
{
    auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
    REQUIRE(device);

    Wayfinder::RenderContext context;
    REQUIRE(context.Initialize(*device, MakeTestConfig()));

    CHECK(&context.GetDevice() == device.get());

    // These verify the returned references are to the context's own members
    // (not default-constructed temporaries). Taking an address of a reference
    // is always non-null, so we just verify they're usable by calling a method.
    CHECK_NOTHROW(context.GetShaders());
    CHECK_NOTHROW(context.GetPipelines());
    CHECK_NOTHROW(context.GetPrograms());
    CHECK_NOTHROW(context.GetTransientBuffers());
    CHECK_NOTHROW(context.GetTransientPool());

    context.Shutdown();
}

TEST_CASE("RenderContext double shutdown is safe")
{
    auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
    REQUIRE(device);

    Wayfinder::RenderContext context;
    REQUIRE(context.Initialize(*device, MakeTestConfig()));

    context.Shutdown();
    context.Shutdown(); // Should not crash
}

TEST_CASE("RenderContext program registry is functional after init")
{
    auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
    REQUIRE(device);

    Wayfinder::RenderContext context;
    REQUIRE(context.Initialize(*device, MakeTestConfig()));

    // Contract: RenderPipeline::Initialise depends on being able to call
    // Register. With NullDevice, pipeline creation fails (no shader files),
    // but the registry itself is functional and doesn't crash.
    Wayfinder::ShaderProgramDesc desc;
    desc.Name = "test_program";
    desc.VertexShaderName = "test_vert";
    desc.FragmentShaderName = "test_frag";
    // Register returns false with NullDevice (no shaders), but must not crash
    context.GetPrograms().Register(desc);

    context.Shutdown();
}
