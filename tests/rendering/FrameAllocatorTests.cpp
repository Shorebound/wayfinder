#include "rendering/ArenaFunction.h"
#include "rendering/FrameAllocator.h"

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Wayfinder::Tests
{
    // ── FrameAllocator ───────────────────────────────────────

    TEST_CASE("FrameAllocator basic allocation returns non-null")
    {
        FrameAllocator allocator;
        void* ptr = allocator.Allocate(64);
        CHECK(ptr != nullptr);
    }

    TEST_CASE("FrameAllocator respects alignment")
    {
        FrameAllocator allocator;

        // Waste a byte to misalign
        allocator.Allocate(1, 1);

        void* aligned = allocator.Allocate(16, 16);
        CHECK(reinterpret_cast<uintptr_t>(aligned) % 16 == 0);

        void* aligned64 = allocator.Allocate(64, 64);
        CHECK(reinterpret_cast<uintptr_t>(aligned64) % 64 == 0);
    }

    TEST_CASE("FrameAllocator multiple allocations do not overlap")
    {
        FrameAllocator allocator;

        auto* a = static_cast<std::byte*>(allocator.Allocate(32));
        auto* b = static_cast<std::byte*>(allocator.Allocate(32));

        // Regions must not overlap: b should start at or after a+32
        CHECK(b >= a + 32);
    }

    TEST_CASE("FrameAllocator Reset brings used bytes to zero")
    {
        FrameAllocator allocator;

        allocator.Allocate(128);
        allocator.Allocate(256);
        CHECK(allocator.GetUsedBytes() > 0);

        allocator.Reset();
        CHECK(allocator.GetUsedBytes() == 0);
    }

    TEST_CASE("FrameAllocator retains capacity across resets")
    {
        FrameAllocator allocator(1024);

        allocator.Allocate(512);
        const size_t cap = allocator.GetCapacity();

        allocator.Reset();
        CHECK(allocator.GetCapacity() == cap);
    }

    TEST_CASE("FrameAllocator grows beyond initial page")
    {
        FrameAllocator allocator(64); // Tiny page

        // Allocate more than one page worth
        allocator.Allocate(128);
        CHECK(allocator.GetCapacity() >= 128);
    }

    TEST_CASE("FrameAllocator Create constructs objects")
    {
        FrameAllocator allocator;

        struct Simple
        {
            int X;
            float Y;
        };

        auto* obj = allocator.Create<Simple>(42, 3.14f);
        CHECK(obj->X == 42);
        CHECK(obj->Y == doctest::Approx(3.14f));
    }

    TEST_CASE("FrameAllocator calls destructors on Reset in LIFO order")
    {
        std::vector<int> destructionOrder;

        struct Tracker
        {
            std::vector<int>& Log;
            int Id;
            ~Tracker() { Log.push_back(Id); }
        };

        {
            FrameAllocator allocator;

            allocator.Create<Tracker>(destructionOrder, 1);
            allocator.Create<Tracker>(destructionOrder, 2);
            allocator.Create<Tracker>(destructionOrder, 3);

            CHECK(destructionOrder.empty());

            allocator.Reset();

            REQUIRE(destructionOrder.size() == 3);
            CHECK(destructionOrder[0] == 3); // LIFO
            CHECK(destructionOrder[1] == 2);
            CHECK(destructionOrder[2] == 1);
        }
    }

    TEST_CASE("FrameAllocator skips destructor for trivially-destructible types")
    {
        FrameAllocator allocator;

        // Trivially destructible — no destructor registered
        allocator.Create<int>(42);
        allocator.Create<float>(1.0f);

        // Just verifying Reset doesn't crash
        allocator.Reset();
    }

    TEST_CASE("FrameAllocator reuse after Reset")
    {
        FrameAllocator allocator(256);

        allocator.Allocate(128);
        allocator.Reset();

        // Should allocate from the same page
        void* ptr = allocator.Allocate(128);
        CHECK(ptr != nullptr);
        CHECK(allocator.GetUsedBytes() == 128);
    }

    // ── ArenaFunction ────────────────────────────────────────

    TEST_CASE("ArenaFunction default-constructed is empty")
    {
        ArenaFunction<void()> fn;
        CHECK_FALSE(static_cast<bool>(fn));
    }

    TEST_CASE("ArenaFunction wrapping a stateless lambda")
    {
        FrameAllocator allocator;
        bool called = false;

        ArenaFunction<void()> fn(allocator,
            [&called]()
            {
                called = true;
            });
        CHECK(static_cast<bool>(fn));

        fn();
        CHECK(called);
    }

    TEST_CASE("ArenaFunction with captures and return value")
    {
        FrameAllocator allocator;

        int base = 10;
        ArenaFunction<int(int)> fn(allocator,
            [base](int x)
            {
                return base + x;
            });

        CHECK(fn(5) == 15);
        CHECK(fn(0) == 10);
    }

    TEST_CASE("ArenaFunction with reference parameters")
    {
        FrameAllocator allocator;

        ArenaFunction<void(int&)> fn(allocator,
            [](int& x)
            {
                x += 100;
            });

        int value = 42;
        fn(value);
        CHECK(value == 142);
    }

    TEST_CASE("ArenaFunction move construction")
    {
        FrameAllocator allocator;

        ArenaFunction<int()> original(allocator,
            []()
            {
                return 99;
            });
        CHECK(static_cast<bool>(original));

        ArenaFunction<int()> moved(std::move(original));
        CHECK(static_cast<bool>(moved));
        CHECK_FALSE(static_cast<bool>(original)); // moved-from is empty

        CHECK(moved() == 99);
    }

    TEST_CASE("ArenaFunction move assignment")
    {
        FrameAllocator allocator;

        ArenaFunction<int()> a(allocator,
            []()
            {
                return 1;
            });
        ArenaFunction<int()> b(allocator,
            []()
            {
                return 2;
            });

        b = std::move(a);
        CHECK(static_cast<bool>(b));
        CHECK_FALSE(static_cast<bool>(a));

        CHECK(b() == 1);
    }

    TEST_CASE("ArenaFunction with non-trivially-destructible capture")
    {
        FrameAllocator allocator;
        int destructorCount = 0;

        struct Counter
        {
            int& Count;
            ~Counter() { ++Count; }
        };

        {
            // The arena stores a moved/copied lambda. Between construction and
            // Reset(), the arena-held destructor should NOT have fired. We track
            // how many destructor calls happen at each stage.
            ArenaFunction<void()> fn(allocator, [c = Counter{destructorCount}]() {});

            // The temporary lambda (+ any intermediate copies) may have been destroyed,
            // so destructorCount may be > 0. Record the baseline.
            const int baseline = destructorCount;

            // The arena still holds one live Counter. Reset should destroy it.
            allocator.Reset();
            CHECK(destructorCount == baseline + 1);
        }
    }

    TEST_CASE("ArenaFunction with large capture")
    {
        FrameAllocator allocator;

        // Simulate a large capture (~256 bytes) typical of render passes
        struct LargeCapture
        {
            float Matrix[16]{};  // 64 bytes
            float Matrix2[16]{}; // 64 bytes
            int Values[32]{};    // 128 bytes
        };

        LargeCapture data;
        data.Values[0] = 42;
        data.Matrix[0] = 1.0f;

        ArenaFunction<int()> fn(allocator,
            [data]()
            {
                return data.Values[0];
            });

        CHECK(fn() == 42);
    }
}
