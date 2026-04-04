#include "plugins/ServiceProvider.h"

#include <doctest/doctest.h>

using namespace Wayfinder;

namespace Wayfinder::Tests
{
    struct NotAServiceProvider
    {
        int value = 0;
    };

    TEST_SUITE("ServiceProvider")
    {
        TEST_CASE("Concept accepts StandaloneServiceProvider")
        {
            static_assert(ServiceProvider<StandaloneServiceProvider>);
        }

        TEST_CASE("Concept rejects non-conforming type")
        {
            static_assert(not ServiceProvider<int>);
            static_assert(not ServiceProvider<NotAServiceProvider>);
        }

        TEST_CASE("Register and Get returns reference")
        {
            StandaloneServiceProvider provider;
            int value = 42;
            provider.Register<int>(value);

            auto& result = provider.Get<int>();
            CHECK(result == 42);

            // Verify it's the same object (reference, not copy)
            value = 99;
            CHECK(result == 99);
        }

        TEST_CASE("TryGet for unregistered returns nullptr")
        {
            StandaloneServiceProvider provider;
            CHECK(provider.TryGet<int>() == nullptr);
        }

        TEST_CASE("Multiple services are independent")
        {
            StandaloneServiceProvider provider;
            int intValue = 10;
            float floatValue = 3.14f;

            provider.Register<int>(intValue);
            provider.Register<float>(floatValue);

            CHECK(provider.Get<int>() == 10);
            CHECK(provider.Get<float>() == doctest::Approx(3.14f));
        }

        TEST_CASE("TryGet returns pointer for registered service")
        {
            StandaloneServiceProvider provider;
            int value = 7;
            provider.Register<int>(value);

            auto* ptr = provider.TryGet<int>();
            REQUIRE(ptr != nullptr);
            CHECK(*ptr == 7);
        }
    }
}
