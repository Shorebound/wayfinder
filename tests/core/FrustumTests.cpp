#include "maths/Bounds.h"
#include "maths/Frustum.h"
#include "maths/Maths.h"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

namespace Wayfinder::Tests
{
    using Wayfinder::AxisAlignedBounds;
    using Wayfinder::BoundingSphere;
    using Wayfinder::ComputeBoundingSphere;
    using Wayfinder::Frustum;
    using Wayfinder::TransformBounds;
    using Wayfinder::TransformSphere;
    using Wayfinder::Maths::LookAt;
    using Wayfinder::Maths::OrthoRH_ZO;
    using Wayfinder::Maths::PerspectiveRH_ZO;

    // ── Helpers ──────────────────────────────────────────────

    /// Camera at the origin, looking down -Z, Y-up, 90° vertical FOV, 1:1 aspect, near=0.1, far=100.
    static Matrix4 MakeDefaultViewProj()
    {
        const Matrix4 view = LookAt({0, 0, 0}, {0, 0, -1}, {0, 1, 0});
        const Matrix4 proj = PerspectiveRH_ZO(std::numbers::pi_v<float> / 2.0f, 1.0f, 0.1f, 100.0f);
        return proj * view;
    }

    // ── TransformBounds ─────────────────────────────────────

    TEST_CASE("TransformBounds with identity preserves bounds")
    {
        const AxisAlignedBounds local{.Min = {-1, -2, -3}, .Max = {1, 2, 3}};
        const AxisAlignedBounds result = TransformBounds(local, Matrix4{1.0f});

        CHECK(result.Min.x == doctest::Approx(-1));
        CHECK(result.Min.y == doctest::Approx(-2));
        CHECK(result.Min.z == doctest::Approx(-3));
        CHECK(result.Max.x == doctest::Approx(1));
        CHECK(result.Max.y == doctest::Approx(2));
        CHECK(result.Max.z == doctest::Approx(3));
    }

    TEST_CASE("TransformBounds with translation shifts bounds")
    {
        const AxisAlignedBounds local{.Min = {-1, -1, -1}, .Max = {1, 1, 1}};
        const Matrix4 translate = Maths::Translate(Matrix4{1.0f}, Float3{10, 20, 30});
        const AxisAlignedBounds result = TransformBounds(local, translate);

        CHECK(result.Min.x == doctest::Approx(9));
        CHECK(result.Min.y == doctest::Approx(19));
        CHECK(result.Min.z == doctest::Approx(29));
        CHECK(result.Max.x == doctest::Approx(11));
        CHECK(result.Max.y == doctest::Approx(21));
        CHECK(result.Max.z == doctest::Approx(31));
    }

    TEST_CASE("TransformBounds with 90-degree Y rotation swaps X and Z extents")
    {
        // A box that is wider in X than Z: 4 units wide (X), 2 units deep (Z).
        const AxisAlignedBounds local{.Min = {-2, -1, -1}, .Max = {2, 1, 1}};

        // 90-degree rotation about Y: X -> -Z, Z -> X.
        const float angle = std::numbers::pi_v<float> / 2.0f;
        const Matrix4 rotate = Maths::Rotate(Matrix4{1.0f}, angle, Float3{0, 1, 0});
        const AxisAlignedBounds result = TransformBounds(local, rotate);

        // After rotation, the 4-unit X extent becomes 4-unit Z extent.
        CHECK(result.Min.x == doctest::Approx(-1).epsilon(1e-5));
        CHECK(result.Max.x == doctest::Approx(1).epsilon(1e-5));
        CHECK(result.Min.z == doctest::Approx(-2).epsilon(1e-5));
        CHECK(result.Max.z == doctest::Approx(2).epsilon(1e-5));
        // Y unchanged.
        CHECK(result.Min.y == doctest::Approx(-1).epsilon(1e-5));
        CHECK(result.Max.y == doctest::Approx(1).epsilon(1e-5));
    }

    TEST_CASE("TransformBounds with uniform scale scales bounds")
    {
        const AxisAlignedBounds local{.Min = {-1, -1, -1}, .Max = {1, 1, 1}};
        const Matrix4 scale = Maths::Scale(Matrix4{1.0f}, Float3{3, 3, 3});
        const AxisAlignedBounds result = TransformBounds(local, scale);

        CHECK(result.Min.x == doctest::Approx(-3));
        CHECK(result.Min.y == doctest::Approx(-3));
        CHECK(result.Min.z == doctest::Approx(-3));
        CHECK(result.Max.x == doctest::Approx(3));
        CHECK(result.Max.y == doctest::Approx(3));
        CHECK(result.Max.z == doctest::Approx(3));
    }

    // ── Frustum plane extraction ────────────────────────────

    TEST_CASE("ExtractPlanes produces normalised planes")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());

        for (const FrustumPlane& plane : frustum.Planes)
        {
            const float len = std::sqrt(Maths::Dot(plane.Normal, plane.Normal));
            CHECK(len == doctest::Approx(1.0f).epsilon(1e-5));
        }
    }

    // ── AABB-frustum tests ──────────────────────────────────

    TEST_CASE("AABB directly in front of camera is visible")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());

        // Small box centred at (0, 0, -5): well inside the frustum.
        const AxisAlignedBounds box{.Min = {-0.5f, -0.5f, -5.5f}, .Max = {0.5f, 0.5f, -4.5f}};
        CHECK(frustum.TestAABB(box));
    }

    TEST_CASE("AABB behind camera is culled")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());

        // Box behind the camera (positive Z in RH).
        const AxisAlignedBounds box{.Min = {-1, -1, 5}, .Max = {1, 1, 10}};
        CHECK_FALSE(frustum.TestAABB(box));
    }

    TEST_CASE("AABB beyond far plane is culled")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());

        // Far plane at z = -100. Put a box at z = -200.
        const AxisAlignedBounds box{.Min = {-1, -1, -201}, .Max = {1, 1, -200}};
        CHECK_FALSE(frustum.TestAABB(box));
    }

    TEST_CASE("AABB far to the right is culled")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());

        // Way off to the right at z = -5.  With 90° FOV and aspect 1:1,
        // the horizontal half-angle is 45°, so at z = -5 the visible width is ~5 units each side.
        const AxisAlignedBounds box{.Min = {50, -1, -5.5f}, .Max = {51, 1, -4.5f}};
        CHECK_FALSE(frustum.TestAABB(box));
    }

    TEST_CASE("AABB far to the left is culled")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());

        const AxisAlignedBounds box{.Min = {-51, -1, -5.5f}, .Max = {-50, 1, -4.5f}};
        CHECK_FALSE(frustum.TestAABB(box));
    }

    TEST_CASE("AABB far above is culled")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());

        const AxisAlignedBounds box{.Min = {-1, 50, -5.5f}, .Max = {1, 51, -4.5f}};
        CHECK_FALSE(frustum.TestAABB(box));
    }

    TEST_CASE("AABB straddling the near plane is visible")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());

        // Partially in front, partially behind the near plane (z = -0.1).
        const AxisAlignedBounds box{.Min = {-0.01f, -0.01f, -0.2f}, .Max = {0.01f, 0.01f, 0.0f}};
        CHECK(frustum.TestAABB(box));
    }

    TEST_CASE("Zero-size AABB at visible location is visible")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());

        // Degenerate (zero-volume) box at a point inside the frustum.
        const AxisAlignedBounds box{.Min = {0, 0, -5}, .Max = {0, 0, -5}};
        CHECK(frustum.TestAABB(box));
    }

    TEST_CASE("Large AABB enclosing the camera is visible")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());

        // Enormous box that fully contains the camera and the entire frustum.
        const AxisAlignedBounds box{.Min = {-1000, -1000, -1000}, .Max = {1000, 1000, 1000}};
        CHECK(frustum.TestAABB(box));
    }

    TEST_CASE("Orthographic frustum culls correctly")
    {
        const Matrix4 view = LookAt({0, 0, 0}, {0, 0, -1}, {0, 1, 0});
        const Matrix4 proj = OrthoRH_ZO(-10, 10, -10, 10, 0.1f, 50.0f);
        const Frustum frustum = Frustum::ExtractPlanes(proj * view);

        // Inside: centred box at z = -5.
        CHECK(frustum.TestAABB({.Min = {-1, -1, -6}, .Max = {1, 1, -4}}));

        // Outside: behind camera.
        CHECK_FALSE(frustum.TestAABB({.Min = {-1, -1, 5}, .Max = {1, 1, 10}}));

        // Outside: beyond far plane.
        CHECK_FALSE(frustum.TestAABB({.Min = {-1, -1, -60}, .Max = {1, 1, -55}}));

        // Outside: to the right of the ortho box (x > 10).
        CHECK_FALSE(frustum.TestAABB({.Min = {15, -1, -6}, .Max = {20, 1, -4}}));
    }

    // ── ComputeBoundingSphere ───────────────────────────────

    TEST_CASE("ComputeBoundingSphere from unit AABB")
    {
        const AxisAlignedBounds box{.Min = {-1, -1, -1}, .Max = {1, 1, 1}};
        const BoundingSphere sphere = ComputeBoundingSphere(box);

        CHECK(sphere.Centre.x == doctest::Approx(0));
        CHECK(sphere.Centre.y == doctest::Approx(0));
        CHECK(sphere.Centre.z == doctest::Approx(0));
        // Half-diagonal of a 2x2x2 cube = sqrt(3) ≈ 1.732.
        CHECK(sphere.Radius == doctest::Approx(std::sqrt(3.0f)).epsilon(1e-5));
    }

    TEST_CASE("ComputeBoundingSphere from offset AABB")
    {
        const AxisAlignedBounds box{.Min = {10, 20, 30}, .Max = {12, 24, 36}};
        const BoundingSphere sphere = ComputeBoundingSphere(box);

        CHECK(sphere.Centre.x == doctest::Approx(11));
        CHECK(sphere.Centre.y == doctest::Approx(22));
        CHECK(sphere.Centre.z == doctest::Approx(33));
        // Half-extents: (1, 2, 3), half-diagonal = sqrt(1+4+9) = sqrt(14).
        CHECK(sphere.Radius == doctest::Approx(std::sqrt(14.0f)).epsilon(1e-5));
    }

    TEST_CASE("ComputeBoundingSphere from zero-volume AABB has zero radius")
    {
        const AxisAlignedBounds box{.Min = {5, 5, 5}, .Max = {5, 5, 5}};
        const BoundingSphere sphere = ComputeBoundingSphere(box);

        CHECK(sphere.Centre.x == doctest::Approx(5));
        CHECK(sphere.Centre.y == doctest::Approx(5));
        CHECK(sphere.Centre.z == doctest::Approx(5));
        CHECK(sphere.Radius == doctest::Approx(0));
    }

    // ── TransformSphere ─────────────────────────────────────

    TEST_CASE("TransformSphere with identity preserves sphere")
    {
        const BoundingSphere sphere{.Centre = {1, 2, 3}, .Radius = 5.0f};
        const BoundingSphere result = TransformSphere(sphere, Matrix4{1.0f});

        CHECK(result.Centre.x == doctest::Approx(1));
        CHECK(result.Centre.y == doctest::Approx(2));
        CHECK(result.Centre.z == doctest::Approx(3));
        CHECK(result.Radius == doctest::Approx(5));
    }

    TEST_CASE("TransformSphere with translation moves centre")
    {
        const BoundingSphere sphere{.Centre = {0, 0, 0}, .Radius = 1.0f};
        const Matrix4 translate = Maths::Translate(Matrix4{1.0f}, Float3{10, 20, 30});
        const BoundingSphere result = TransformSphere(sphere, translate);

        CHECK(result.Centre.x == doctest::Approx(10));
        CHECK(result.Centre.y == doctest::Approx(20));
        CHECK(result.Centre.z == doctest::Approx(30));
        CHECK(result.Radius == doctest::Approx(1));
    }

    TEST_CASE("TransformSphere with uniform scale scales radius")
    {
        const BoundingSphere sphere{.Centre = {0, 0, 0}, .Radius = 2.0f};
        const Matrix4 scale = Maths::Scale(Matrix4{1.0f}, Float3{3, 3, 3});
        const BoundingSphere result = TransformSphere(sphere, scale);

        CHECK(result.Centre.x == doctest::Approx(0));
        CHECK(result.Centre.y == doctest::Approx(0));
        CHECK(result.Centre.z == doctest::Approx(0));
        CHECK(result.Radius == doctest::Approx(6));
    }

    TEST_CASE("TransformSphere with non-uniform scale uses max axis")
    {
        const BoundingSphere sphere{.Centre = {0, 0, 0}, .Radius = 1.0f};
        // Scale (1, 5, 2): max axis length = 5, so radius should be 5.
        const Matrix4 scale = Maths::Scale(Matrix4{1.0f}, Float3{1, 5, 2});
        const BoundingSphere result = TransformSphere(sphere, scale);

        CHECK(result.Radius == doctest::Approx(5));
    }

    // ── Frustum sphere tests ────────────────────────────────

    TEST_CASE("TestSphere: sphere in front of camera is visible")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());
        CHECK(frustum.TestSphere({.Centre = {0, 0, -5}, .Radius = 1.0f}));
    }

    TEST_CASE("TestSphere: sphere behind camera is culled")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());
        CHECK_FALSE(frustum.TestSphere({.Centre = {0, 0, 5}, .Radius = 1.0f}));
    }

    TEST_CASE("TestSphere: sphere beyond far plane is culled")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());
        CHECK_FALSE(frustum.TestSphere({.Centre = {0, 0, -200}, .Radius = 1.0f}));
    }

    TEST_CASE("TestSphere: sphere far off to side is culled")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());
        CHECK_FALSE(frustum.TestSphere({.Centre = {100, 0, -5}, .Radius = 1.0f}));
    }

    TEST_CASE("TestSphere: large sphere enclosing camera is visible")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());
        CHECK(frustum.TestSphere({.Centre = {0, 0, 0}, .Radius = 500.0f}));
    }

    // ── Two-tier TestBounds ─────────────────────────────────

    TEST_CASE("TestBounds: visible object passes both tiers")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());
        const AxisAlignedBounds box{.Min = {-0.5f, -0.5f, -5.5f}, .Max = {0.5f, 0.5f, -4.5f}};
        const BoundingSphere sphere = ComputeBoundingSphere(box);

        CHECK(frustum.TestBounds(sphere, box));
    }

    TEST_CASE("TestBounds: object behind camera is culled by sphere tier")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());
        const AxisAlignedBounds box{.Min = {-1, -1, 5}, .Max = {1, 1, 10}};
        const BoundingSphere sphere = ComputeBoundingSphere(box);

        CHECK_FALSE(frustum.TestBounds(sphere, box));
    }

    TEST_CASE("TestBounds: object far off to side is culled")
    {
        const Frustum frustum = Frustum::ExtractPlanes(MakeDefaultViewProj());
        const AxisAlignedBounds box{.Min = {50, -1, -5.5f}, .Max = {51, 1, -4.5f}};
        const BoundingSphere sphere = ComputeBoundingSphere(box);

        CHECK_FALSE(frustum.TestBounds(sphere, box));
    }

} // namespace Wayfinder::Tests
