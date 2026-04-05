#pragma once

#include <cstddef>
#include <vector>

#include "RenderFrame.h"

namespace Wayfinder
{
    /**
     * @brief Scene render-data collector.
     *
     * Accepts mesh, light, and view submissions from game states or systems.
     * Pure data struct with no ECS/flecs dependency. Reuses vocabulary types
     * from RenderFrame.h.
     *
     * DebugDraw on SceneCanvas is for scene-attached debug visualisation.
     * The separate DebugCanvas is for standalone debug primitives (gizmos,
     * wireframes, world grid). Both feed distinct render features.
     */
    struct SceneCanvas
    {
        std::vector<RenderView> Views;
        std::vector<RenderMeshSubmission> Meshes;
        std::vector<RenderLightSubmission> Lights;
        RenderDebugDrawList DebugDraw;
        BlendableEffectStack PostProcess;

        /**
         * @brief Adds a view and returns its index.
         */
        auto AddView(RenderView view) -> size_t
        {
            Views.push_back(std::move(view));
            return Views.size() - 1;
        }

        /**
         * @brief Submits a mesh for rendering.
         */
        void SubmitMesh(RenderMeshSubmission submission)
        {
            Meshes.push_back(std::move(submission));
        }

        /**
         * @brief Submits a light for rendering.
         */
        void SubmitLight(RenderLightSubmission light)
        {
            Lights.push_back(std::move(light));
        }

        /**
         * @brief Clears all submissions. Vector capacity is preserved (std::vector::clear).
         */
        void Clear()
        {
            Views.clear();
            Meshes.clear();
            Lights.clear();
            DebugDraw = RenderDebugDrawList{};
            PostProcess = BlendableEffectStack{};
        }
    };

    /**
     * @brief UI render-data collector.
     *
     * Tracks whether overlays produced ImGui output this frame. The actual
     * ImDrawData* is retrieved from ImGui::GetDrawData() by the UI render
     * feature -- UICanvas just records presence.
     */
    struct UICanvas
    {
        /**
         * @brief Records that ImGui draw data was produced this frame.
         */
        void CaptureImGuiDrawData()
        {
            m_hasImGuiData = true;
        }

        /**
         * @brief Returns whether ImGui draw data is available.
         */
        [[nodiscard]] auto HasImGuiData() const -> bool
        {
            return m_hasImGuiData;
        }

        /**
         * @brief Resets for the next frame.
         */
        void Clear()
        {
            m_hasImGuiData = false;
        }

    private:
        bool m_hasImGuiData = false;
    };

    /**
     * @brief Debug render-data collector.
     *
     * Accepts standalone debug primitives (lines, boxes) and world-grid
     * settings. Separate from SceneCanvas::DebugDraw which is for
     * scene-attached debug vis.
     */
    struct DebugCanvas
    {
        std::vector<RenderDebugLine> Lines;
        std::vector<RenderDebugBox> Boxes;
        bool ShowWorldGrid = false;
        int WorldGridSlices = 100;
        float WorldGridSpacing = 1.0f;

        /**
         * @brief Submits a debug line.
         */
        void SubmitLine(RenderDebugLine line)
        {
            Lines.push_back(std::move(line));
        }

        /**
         * @brief Submits a debug box.
         */
        void SubmitBox(RenderDebugBox box)
        {
            Boxes.push_back(std::move(box));
        }

        /**
         * @brief Clears all primitives and resets grid settings to defaults.
         */
        void Clear()
        {
            Lines.clear();
            Boxes.clear();
            ShowWorldGrid = false;
            WorldGridSlices = 100;
            WorldGridSpacing = 1.0f;
        }
    };

    /**
     * @brief Aggregates all canvas types for a single frame.
     *
     * Owned by the application or state; passed to the renderer each frame.
     * Reset() clears all canvases for the next frame without deallocating
     * backing memory.
     */
    struct FrameCanvases
    {
        SceneCanvas Scene;
        UICanvas UI;
        DebugCanvas Debug;

        /**
         * @brief Resets all canvases for the next frame.
         */
        void Reset()
        {
            Scene.Clear();
            UI.Clear();
            Debug.Clear();
        }
    };

} // namespace Wayfinder
