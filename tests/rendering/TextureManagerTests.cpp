#include "rendering/RenderTypes.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/resources/TextureManager.h"

#include <doctest/doctest.h>

namespace Wayfinder::Tests
{
    // ── Mip Level Calculation ────────────────────────────────

    TEST_CASE("CalculateMipLevels returns correct count for power-of-two dimensions")
    {
        CHECK(CalculateMipLevels(1, 1) == 1);
        CHECK(CalculateMipLevels(2, 2) == 2);
        CHECK(CalculateMipLevels(4, 4) == 3);
        CHECK(CalculateMipLevels(256, 256) == 9);
        CHECK(CalculateMipLevels(1024, 1024) == 11);
    }

    TEST_CASE("CalculateMipLevels handles non-power-of-two dimensions")
    {
        CHECK(CalculateMipLevels(3, 3) == 2);
        CHECK(CalculateMipLevels(5, 5) == 3);
        CHECK(CalculateMipLevels(100, 100) == 7);
        CHECK(CalculateMipLevels(1920, 1080) == 11);
    }

    TEST_CASE("CalculateMipLevels uses larger dimension for non-square textures")
    {
        CHECK(CalculateMipLevels(512, 1) == 10);
        CHECK(CalculateMipLevels(1, 512) == 10);
        CHECK(CalculateMipLevels(256, 128) == 9);
    }

    TEST_CASE("CalculateMipLevels returns 1 for zero dimensions")
    {
        CHECK(CalculateMipLevels(0, 0) == 1);
        CHECK(CalculateMipLevels(0, 256) == 1);
        CHECK(CalculateMipLevels(256, 0) == 1);
    }

    // ── TextureCreateDesc defaults ───────────────────────────

    TEST_CASE("TextureCreateDesc defaults to 1 mip level")
    {
        TextureCreateDesc desc;
        CHECK(desc.MipLevels == 1);
    }

    // ── SamplerCreateDesc defaults ───────────────────────────

    TEST_CASE("SamplerCreateDesc defaults include mipmap fields")
    {
        SamplerCreateDesc desc;
        CHECK(desc.MipmapMode == SamplerMipmapMode::Nearest);
        CHECK(desc.MinLod == 0.0f);
        CHECK(desc.MaxLod == 1000.0f);
    }

    // ── TextureManager ───────────────────────────────────────

    TEST_CASE("TextureManager initialises with NullDevice")
    {
        auto device = RenderDevice::Create(RenderBackend::Null);
        REQUIRE(device);

        TextureManager manager;
        // NullDevice now returns distinguishable handles, so Initialise should succeed.
        const bool result = manager.Initialise(*device);
        CHECK(result);
        manager.Shutdown();
    }

    TEST_CASE("TextureManager Shutdown is safe on uninitialised manager")
    {
        TextureManager manager;
        CHECK_NOTHROW(manager.Shutdown());
    }

    TEST_CASE("TextureManager Shutdown is safe when called twice")
    {
        auto device = RenderDevice::Create(RenderBackend::Null);
        REQUIRE(device);

        TextureManager manager;
        manager.Initialise(*device);
        manager.Shutdown();
        CHECK_NOTHROW(manager.Shutdown());
    }

    TEST_CASE("TextureManager sampler deduplication returns consistent handles")
    {
        auto device = RenderDevice::Create(RenderBackend::Null);
        REQUIRE(device);

        TextureManager manager;
        REQUIRE(manager.Initialise(*device));

        SamplerCreateDesc desc;
        desc.MinFilter = SamplerFilter::Linear;
        desc.MagFilter = SamplerFilter::Linear;
        desc.AddressModeU = SamplerAddressMode::Repeat;
        desc.AddressModeV = SamplerAddressMode::Repeat;

        GPUSamplerHandle first = manager.GetOrCreateSampler(desc);
        GPUSamplerHandle second = manager.GetOrCreateSampler(desc);
        CHECK(first == second);

        manager.Shutdown();
    }

    TEST_CASE("TextureManager sampler deduplication differentiates distinct descs")
    {
        auto device = RenderDevice::Create(RenderBackend::Null);
        REQUIRE(device);

        TextureManager manager;
        REQUIRE(manager.Initialise(*device));

        SamplerCreateDesc nearestRepeat;
        nearestRepeat.MinFilter = SamplerFilter::Nearest;
        nearestRepeat.MagFilter = SamplerFilter::Nearest;
        nearestRepeat.AddressModeU = SamplerAddressMode::Repeat;
        nearestRepeat.AddressModeV = SamplerAddressMode::Repeat;

        SamplerCreateDesc linearClamp;
        linearClamp.MinFilter = SamplerFilter::Linear;
        linearClamp.MagFilter = SamplerFilter::Linear;
        linearClamp.AddressModeU = SamplerAddressMode::ClampToEdge;
        linearClamp.AddressModeV = SamplerAddressMode::ClampToEdge;

        GPUSamplerHandle a = manager.GetOrCreateSampler(nearestRepeat);
        GPUSamplerHandle b = manager.GetOrCreateSampler(linearClamp);

        // NullDevice now returns distinguishable handles — verify they differ.
        CHECK(a != b);

        // Re-fetch should hit cache and return the same handles.
        CHECK(manager.GetOrCreateSampler(nearestRepeat) == a);
        CHECK(manager.GetOrCreateSampler(linearClamp) == b);

        manager.Shutdown();
    }

    TEST_CASE("TextureManager sampler hash differentiates mipmap modes")
    {
        auto device = RenderDevice::Create(RenderBackend::Null);
        REQUIRE(device);
        TextureManager manager;
        REQUIRE(manager.Initialise(*device));

        SamplerCreateDesc descA;
        descA.MinFilter = SamplerFilter::Linear;
        descA.MagFilter = SamplerFilter::Linear;
        descA.MipmapMode = SamplerMipmapMode::Nearest;

        SamplerCreateDesc descB = descA;
        descB.MipmapMode = SamplerMipmapMode::Linear;

        GPUSamplerHandle a = manager.GetOrCreateSampler(descA);
        GPUSamplerHandle b = manager.GetOrCreateSampler(descB);

        // Distinct mipmap modes must produce distinct handles.
        CHECK(a != b);

        manager.Shutdown();
    }

    TEST_CASE("SamplerCreateDesc defaults include anisotropy fields")
    {
        SamplerCreateDesc desc;
        CHECK(desc.MipLodBias == 0.0f);
        CHECK_FALSE(desc.EnableAnisotropy);
        CHECK(desc.MaxAnisotropy == 1.0f);
    }

    TEST_CASE("TextureManager sampler hash differentiates anisotropy")
    {
        auto device = RenderDevice::Create(RenderBackend::Null);
        REQUIRE(device);
        TextureManager manager;
        REQUIRE(manager.Initialise(*device));

        SamplerCreateDesc descA;
        descA.MinFilter = SamplerFilter::Linear;
        descA.MagFilter = SamplerFilter::Linear;
        descA.MipmapMode = SamplerMipmapMode::Linear;
        descA.EnableAnisotropy = false;

        SamplerCreateDesc descB = descA;
        descB.EnableAnisotropy = true;
        descB.MaxAnisotropy = 4.0f;

        GPUSamplerHandle a = manager.GetOrCreateSampler(descA);
        GPUSamplerHandle b = manager.GetOrCreateSampler(descB);

        CHECK(a != b);

        manager.Shutdown();
    }

    TEST_CASE("TextureManager fallback accessors return default handles before init")
    {
        const TextureManager manager;
        CHECK_FALSE(manager.GetFallback().IsValid());
        CHECK_FALSE(manager.GetWhite().IsValid());
        CHECK_FALSE(manager.GetBlack().IsValid());
        CHECK_FALSE(manager.GetFlatNormal().IsValid());
    }

} // namespace Wayfinder::Tests
