#include "core/Handle.h"
#include "core/ResourcePool.h"
#include "rendering/backend/GPUHandles.h"

#include <doctest/doctest.h>

#include <type_traits>
#include <unordered_set>

namespace Wayfinder::Tests
{
    struct TestTag {};
    using TestHandle = Wayfinder::Handle<TestTag>;
    using TestPool = Wayfinder::ResourcePool<TestTag, int>;

    struct OtherTag {};
    using OtherHandle = Wayfinder::Handle<OtherTag>;

    // Compile-time proof that Handle<A> and Handle<B> are distinct types.
    static_assert(!std::is_same_v<TestHandle, OtherHandle>,
                  "Handles with different tags must be distinct types");

    // Compile-time proof that RenderMeshHandle is distinct from all GPU handle types.
    static_assert(!std::is_same_v<Wayfinder::RenderMeshHandle, Wayfinder::GPUBufferHandle>,
                  "RenderMeshHandle must not be the same type as GPUBufferHandle");
    static_assert(!std::is_same_v<Wayfinder::RenderMeshHandle, Wayfinder::GPUTextureHandle>,
                  "RenderMeshHandle must not be the same type as GPUTextureHandle");
    static_assert(!std::is_same_v<Wayfinder::RenderMeshHandle, Wayfinder::GPUShaderHandle>,
                  "RenderMeshHandle must not be the same type as GPUShaderHandle");
    static_assert(!std::is_same_v<Wayfinder::RenderMeshHandle, Wayfinder::GPUPipelineHandle>,
                  "RenderMeshHandle must not be the same type as GPUPipelineHandle");

    // Compile-time proof that RenderMaterialHandle is distinct from all GPU handle types and RenderMeshHandle.
    static_assert(!std::is_same_v<Wayfinder::RenderMaterialHandle, Wayfinder::GPUBufferHandle>,
                  "RenderMaterialHandle must not be the same type as GPUBufferHandle");
    static_assert(!std::is_same_v<Wayfinder::RenderMaterialHandle, Wayfinder::GPUTextureHandle>,
                  "RenderMaterialHandle must not be the same type as GPUTextureHandle");
    static_assert(!std::is_same_v<Wayfinder::RenderMaterialHandle, Wayfinder::GPUShaderHandle>,
                  "RenderMaterialHandle must not be the same type as GPUShaderHandle");
    static_assert(!std::is_same_v<Wayfinder::RenderMaterialHandle, Wayfinder::GPUPipelineHandle>,
                  "RenderMaterialHandle must not be the same type as GPUPipelineHandle");
    static_assert(!std::is_same_v<Wayfinder::RenderMaterialHandle, Wayfinder::RenderMeshHandle>,
                  "RenderMaterialHandle must not be the same type as RenderMeshHandle");
}

// ── Handle Basics ────────────────────────────────────────

TEST_CASE("Default-constructed handle is invalid")
{
    TestHandle h{};
    CHECK_FALSE(h.IsValid());
    CHECK_FALSE(static_cast<bool>(h));
}

TEST_CASE("Handle::Invalid() returns an invalid handle")
{
    auto h = TestHandle::Invalid();
    CHECK_FALSE(h.IsValid());
    CHECK(+h.Index == 0);
    CHECK(+h.Generation == 0);
}

TEST_CASE("Handle with non-zero generation is valid")
{
    TestHandle h{};
    h.Index = 5;
    h.Generation = 1;
    CHECK(h.IsValid());
    CHECK(static_cast<bool>(h));
}

TEST_CASE("Handle equality and comparison")
{
    TestHandle a{};
    a.Index = 3;
    a.Generation = 7;

    TestHandle b{};
    b.Index = 3;
    b.Generation = 7;

    TestHandle c{};
    c.Index = 3;
    c.Generation = 8;

    CHECK(a == b);
    CHECK(a != c);
}

TEST_CASE("Handle hashing works in unordered containers")
{
    TestHandle a{};
    a.Index = 1;
    a.Generation = 1;

    TestHandle b{};
    b.Index = 2;
    b.Generation = 1;

    std::unordered_set<TestHandle> set;
    set.insert(a);
    set.insert(b);
    set.insert(a); // duplicate

    CHECK(set.size() == 2);
    CHECK(set.contains(a));
    CHECK(set.contains(b));
}

// ── ResourcePool Basics ──────────────────────────────────

TEST_CASE("Pool acquire returns a valid handle")
{
    TestPool pool;
    auto handle = pool.Acquire(42);

    CHECK(handle.IsValid());
    CHECK(pool.IsValid(handle));
    CHECK(pool.ActiveCount() == 1);
}

TEST_CASE("Pool get returns the stored resource")
{
    TestPool pool;
    auto handle = pool.Acquire(99);

    auto* value = pool.Get(handle);
    REQUIRE(value != nullptr);
    CHECK(*value == 99);
}

TEST_CASE("Pool get on invalid handle returns nullptr")
{
    TestPool pool;
    CHECK(pool.Get(TestHandle::Invalid()) == nullptr);
    CHECK(pool.Get(TestHandle{}) == nullptr);
}

TEST_CASE("Pool release invalidates the handle")
{
    TestPool pool;
    auto handle = pool.Acquire(10);

    pool.Release(handle);

    CHECK_FALSE(pool.IsValid(handle));
    CHECK(pool.Get(handle) == nullptr);
    CHECK(pool.ActiveCount() == 0);
}

// ── Generation Bumping ───────────────────────────────────

TEST_CASE("Stale handle is rejected after release and re-acquire")
{
    TestPool pool;

    auto first = pool.Acquire(100);
    pool.Release(first);

    auto second = pool.Acquire(200);

    // Same slot index, but different generation
    CHECK(+first.Index == +second.Index);
    CHECK(+first.Generation != +second.Generation);

    // Old handle no longer valid
    CHECK_FALSE(pool.IsValid(first));
    CHECK(pool.Get(first) == nullptr);

    // New handle works
    auto* value = pool.Get(second);
    REQUIRE(value != nullptr);
    CHECK(*value == 200);
}

TEST_CASE("Multiple release-reacquire cycles bump generation")
{
    TestPool pool;

    auto h1 = pool.Acquire(1);
    auto gen1 = h1.Generation;
    pool.Release(h1);

    auto h2 = pool.Acquire(2);
    auto gen2 = h2.Generation;
    pool.Release(h2);

    auto h3 = pool.Acquire(3);
    auto gen3 = h3.Generation;

    // All should use the same index (slot reuse via free list)
    CHECK(+h1.Index == +h2.Index);
    CHECK(+h2.Index == +h3.Index);

    // Generations should all differ
    CHECK(gen1 != gen2);
    CHECK(gen2 != gen3);
    CHECK(gen1 != gen3);
}

// ── Pool Growth ──────────────────────────────────────────

TEST_CASE("Pool grows as handles are acquired")
{
    TestPool pool;

    constexpr int COUNT = 100;
    std::vector<TestHandle> handles;
    handles.reserve(COUNT);

    for (int i = 0; i < COUNT; ++i)
    {
        handles.push_back(pool.Acquire(i));
    }

    CHECK(pool.ActiveCount() == COUNT);

    for (int i = 0; i < COUNT; ++i)
    {
        auto* value = pool.Get(handles[i]);
        REQUIRE(value != nullptr);
        CHECK(*value == i);
    }
}

TEST_CASE("Released slots are reused before growing")
{
    TestPool pool;

    auto h0 = pool.Acquire(0);
    auto h1 = pool.Acquire(1);
    auto h2 = pool.Acquire(2);

    pool.Release(h1); // Free slot 1

    auto h3 = pool.Acquire(3);
    CHECK(+h3.Index == +h1.Index); // Reuses slot 1

    pool.Release(h0);
    auto h4 = pool.Acquire(4);
    CHECK(+h4.Index == +h0.Index); // Reuses slot 0
}

// ── Pool Clear ───────────────────────────────────────────

TEST_CASE("Pool clear invalidates all handles")
{
    TestPool pool;

    auto h1 = pool.Acquire(10);
    auto h2 = pool.Acquire(20);
    auto h3 = pool.Acquire(30);

    pool.Clear();

    CHECK(pool.ActiveCount() == 0);
    CHECK_FALSE(pool.IsValid(h1));
    CHECK_FALSE(pool.IsValid(h2));
    CHECK_FALSE(pool.IsValid(h3));
}

// ── Const Access ─────────────────────────────────────────

TEST_CASE("Const pool access returns const pointer")
{
    TestPool pool;
    auto handle = pool.Acquire(42);

    const TestPool& constPool = pool;
    const int* value = constPool.Get(handle);
    REQUIRE(value != nullptr);
    CHECK(*value == 42);
}

// ── Pool with Non-Trivial Resource ───────────────────────

TEST_CASE("Pool works with move-only types")
{
    struct MoveOnly
    {
        int Value = 0;
        MoveOnly() = default;
        explicit MoveOnly(int v) : Value(v) {}
        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;
        MoveOnly(MoveOnly&& other) noexcept : Value(other.Value) { other.Value = -1; }
        MoveOnly& operator=(MoveOnly&& other) noexcept { Value = other.Value; other.Value = -1; return *this; }
    };

    Wayfinder::ResourcePool<TestTag, MoveOnly> pool;
    auto handle = pool.Acquire(MoveOnly{77});

    auto* ptr = pool.Get(handle);
    REQUIRE(ptr != nullptr);
    CHECK(ptr->Value == 77);

    pool.Release(handle);
    CHECK(pool.ActiveCount() == 0);
}
