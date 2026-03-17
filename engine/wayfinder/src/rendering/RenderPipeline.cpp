#include "RenderPipeline.h"

#include "../core/Log.h"
#include "RenderAPI.h"
#include "RenderResources.h"

#include <algorithm>
#include <tuple>

namespace Wayfinder
{
    void RenderPipeline::Execute(const RenderFrame& frame, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        if (frame.Views.empty())
        {
            return;
        }

        if (frame.Views.size() > 1)
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline currently supports a single active view with the Raylib backend. Rendering only the first view out of {0}.", frame.Views.size());
        }

        for (const RenderPass& pass : frame.Passes)
        {
            ExecutePass(pass, frame, renderAPI, resources);
        }
    }

    void RenderPipeline::ExecutePass(const RenderPass& pass, const RenderFrame& frame, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        if (!pass.Enabled)
        {
            return;
        }

        if (pass.ViewIndex >= frame.Views.size())
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline skipped pass because it referenced missing view index {0} for frame '{1}'.", pass.ViewIndex, frame.SceneName);
            return;
        }

        const RenderView& view = frame.Views[pass.ViewIndex];

        switch (pass.Kind)
        {
        case RenderPassKind::Scene:
            ExecuteScenePass(pass, frame, view, renderAPI, resources);
            break;
        case RenderPassKind::Debug:
            ExecuteDebugPass(frame, view, renderAPI);
            break;
        }
    }

    void RenderPipeline::ExecuteScenePass(const RenderPass& pass, const RenderFrame& frame, const RenderView& view, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        renderAPI.Begin3DMode(view.CameraState);

        std::vector<const RenderMeshSubmission*> sortedMeshes;
        sortedMeshes.reserve(frame.Meshes.size());
        for (const RenderMeshSubmission& mesh : frame.Meshes)
        {
            if (!mesh.Visible)
            {
                continue;
            }

            if (pass.SceneLayer && mesh.Layer != *pass.SceneLayer)
            {
                continue;
            }

            sortedMeshes.push_back(&mesh);
        }

        std::stable_sort(sortedMeshes.begin(), sortedMeshes.end(), [](const RenderMeshSubmission* lhs, const RenderMeshSubmission* rhs)
        {
            return std::tie(lhs->Layer, lhs->SortPriority, lhs->SortKey) < std::tie(rhs->Layer, rhs->SortPriority, rhs->SortKey);
        });

        for (const RenderMeshSubmission* mesh : sortedMeshes)
        {
            DrawMeshSubmission(*mesh, renderAPI, resources);
        }

        renderAPI.End3DMode();
    }

    void RenderPipeline::ExecuteDebugPass(const RenderFrame& frame, const RenderView& view, IRenderAPI& renderAPI) const
    {
        renderAPI.Begin3DMode(view.CameraState);

        if (frame.Debug.ShowWorldGrid)
        {
            renderAPI.DrawGrid(frame.Debug.WorldGridSlices, frame.Debug.WorldGridSpacing);
        }

        for (const RenderDebugLine& debugLine : frame.Debug.Lines)
        {
            renderAPI.DrawLine3D(debugLine.Start, debugLine.End, debugLine.Color);
        }

        for (const RenderDebugBox& debugBox : frame.Debug.Boxes)
        {
            DrawDebugBox(debugBox, renderAPI);
        }

        renderAPI.End3DMode();
    }

    void RenderPipeline::DrawMeshSubmission(const RenderMeshSubmission& mesh, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        const RenderMeshResource& resource = resources.ResolveMesh(mesh);

        switch (resource.Geometry.Type)
        {
        case RenderGeometryType::Box:
            ApplyMaterialBinding(mesh.Material, mesh.LocalToWorld, resource.Geometry.Dimensions, renderAPI);
            break;
        }
    }

    void RenderPipeline::DrawDebugBox(const RenderDebugBox& debugBox, IRenderAPI& renderAPI) const
    {
        ApplyMaterialBinding(debugBox.Material, debugBox.LocalToWorld, debugBox.Dimensions, renderAPI);
    }

    void RenderPipeline::ApplyMaterialBinding(const RenderMaterialBinding& binding, const Matrix4& transform, const Float3& dimensions, IRenderAPI& renderAPI) const
    {
        switch (binding.FillMode)
        {
        case RenderFillMode::Solid:
            renderAPI.DrawBox(transform, dimensions, binding.BaseColor);
            break;
        case RenderFillMode::Wireframe:
            renderAPI.DrawBoxWires(transform, dimensions, binding.WireframeColor);
            break;
        case RenderFillMode::SolidAndWireframe:
            renderAPI.DrawBox(transform, dimensions, binding.BaseColor);
            renderAPI.DrawBoxWires(transform, dimensions, binding.WireframeColor);
            break;
        }
    }
} // namespace Wayfinder