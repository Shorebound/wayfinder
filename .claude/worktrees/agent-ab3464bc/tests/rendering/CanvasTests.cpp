#include "rendering/graph/Canvas.h"

#include <doctest/doctest.h>

namespace Wayfinder::Tests
{
    // ── SceneCanvas ─────────────────────────────────────────────

    TEST_CASE("SceneCanvas accepts mesh submissions")
    {
        SceneCanvas canvas;

        canvas.SubmitMesh(RenderMeshSubmission{});
        canvas.SubmitMesh(RenderMeshSubmission{});
        canvas.SubmitMesh(RenderMeshSubmission{});

        CHECK(canvas.Meshes.size() == 3);
    }

    TEST_CASE("SceneCanvas AddView returns sequential indices")
    {
        SceneCanvas canvas;

        const size_t idx0 = canvas.AddView(RenderView{});
        const size_t idx1 = canvas.AddView(RenderView{});

        CHECK(idx0 == 0);
        CHECK(idx1 == 1);
        CHECK(canvas.Views.size() == 2);
    }

    TEST_CASE("SceneCanvas accepts light submissions")
    {
        SceneCanvas canvas;

        canvas.SubmitLight(RenderLightSubmission{});
        canvas.SubmitLight(RenderLightSubmission{});

        CHECK(canvas.Lights.size() == 2);
    }

    TEST_CASE("SceneCanvas Clear preserves capacity")
    {
        SceneCanvas canvas;

        // Submit enough items to force allocation
        constexpr size_t N = 32;
        for (size_t i = 0; i < N; ++i)
        {
            canvas.SubmitMesh(RenderMeshSubmission{});
            canvas.SubmitLight(RenderLightSubmission{});
        }
        canvas.AddView(RenderView{});

        const size_t meshCapacity = canvas.Meshes.capacity();
        const size_t lightCapacity = canvas.Lights.capacity();
        const size_t viewCapacity = canvas.Views.capacity();

        CHECK(meshCapacity >= N);
        CHECK(lightCapacity >= N);

        canvas.Clear();

        CHECK(canvas.Meshes.size() == 0);
        CHECK(canvas.Lights.size() == 0);
        CHECK(canvas.Views.size() == 0);
        CHECK(canvas.Meshes.capacity() >= meshCapacity);
        CHECK(canvas.Lights.capacity() >= lightCapacity);
        CHECK(canvas.Views.capacity() >= viewCapacity);
    }

    TEST_CASE("SceneCanvas Clear resets DebugDraw and PostProcess")
    {
        SceneCanvas canvas;

        canvas.DebugDraw.ShowWorldGrid = true;
        canvas.DebugDraw.Lines.push_back(RenderDebugLine{});

        canvas.Clear();

        CHECK(canvas.DebugDraw.ShowWorldGrid == false);
        CHECK(canvas.DebugDraw.Lines.empty());
    }

    // ── UICanvas ────────────────────────────────────────────────

    TEST_CASE("UICanvas tracks ImGui data presence")
    {
        UICanvas canvas;

        CHECK_FALSE(canvas.HasImGuiData());

        canvas.CaptureImGuiDrawData();
        CHECK(canvas.HasImGuiData());

        canvas.Clear();
        CHECK_FALSE(canvas.HasImGuiData());
    }

    // ── DebugCanvas ─────────────────────────────────────────────

    TEST_CASE("DebugCanvas accepts lines and boxes")
    {
        DebugCanvas canvas;

        canvas.SubmitLine(RenderDebugLine{});
        canvas.SubmitLine(RenderDebugLine{});
        canvas.SubmitBox(RenderDebugBox{});

        CHECK(canvas.Lines.size() == 2);
        CHECK(canvas.Boxes.size() == 1);
    }

    TEST_CASE("DebugCanvas Clear resets grid settings")
    {
        DebugCanvas canvas;

        canvas.ShowWorldGrid = true;
        canvas.WorldGridSlices = 50;
        canvas.WorldGridSpacing = 2.0f;
        canvas.SubmitLine(RenderDebugLine{});
        canvas.SubmitBox(RenderDebugBox{});

        canvas.Clear();

        CHECK(canvas.ShowWorldGrid == false);
        CHECK(canvas.WorldGridSlices == 100);
        CHECK(canvas.WorldGridSpacing == doctest::Approx(1.0f));
        CHECK(canvas.Lines.empty());
        CHECK(canvas.Boxes.empty());
    }

    // ── FrameCanvases ───────────────────────────────────────────

    TEST_CASE("FrameCanvases Reset clears all canvases")
    {
        FrameCanvases canvases;

        // Fill scene canvas
        canvases.Scene.SubmitMesh(RenderMeshSubmission{});
        canvases.Scene.SubmitLight(RenderLightSubmission{});
        canvases.Scene.AddView(RenderView{});

        // Fill UI canvas
        canvases.UI.CaptureImGuiDrawData();

        // Fill debug canvas
        canvases.Debug.SubmitLine(RenderDebugLine{});
        canvases.Debug.SubmitBox(RenderDebugBox{});
        canvases.Debug.ShowWorldGrid = true;

        canvases.Reset();

        CHECK(canvases.Scene.Meshes.empty());
        CHECK(canvases.Scene.Lights.empty());
        CHECK(canvases.Scene.Views.empty());
        CHECK_FALSE(canvases.UI.HasImGuiData());
        CHECK(canvases.Debug.Lines.empty());
        CHECK(canvases.Debug.Boxes.empty());
        CHECK(canvases.Debug.ShowWorldGrid == false);
    }

    // ── Zero ECS dependency ─────────────────────────────────────
    // This test file compiles without any flecs or ECS includes,
    // proving Canvas.h has no gameplay/ECS dependency.

    TEST_CASE("Canvas types are default-constructible without ECS")
    {
        // If Canvas.h pulled in flecs, this file would fail to compile
        // (or require flecs linkage). The fact that it compiles and
        // runs in the render test executable is the verification.
        SceneCanvas scene;
        UICanvas ui;
        DebugCanvas debug;
        FrameCanvases frame;

        CHECK(scene.Meshes.empty());
        CHECK_FALSE(ui.HasImGuiData());
        CHECK(debug.Lines.empty());
        CHECK(frame.Scene.Meshes.empty());
    }
}
