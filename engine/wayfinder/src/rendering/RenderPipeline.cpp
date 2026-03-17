#include "RenderPipeline.h"

#include "../core/Log.h"
#include "RenderAPI.h"
#include "RenderResources.h"

#include <algorithm>
#include <unordered_set>
#include <tuple>

namespace Wayfinder
{
    void RenderPipeline::Execute(const RenderFrame& frame, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        const RenderBackendCapabilities& capabilities = renderAPI.GetCapabilities();

        if (frame.Views.empty())
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline skipped frame '{0}' because it has no extracted views.", frame.SceneName);
            return;
        }

        if (frame.Passes.empty())
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline skipped frame '{0}' because it has no extracted render passes.", frame.SceneName);
            return;
        }

        if (frame.Views.size() > capabilities.MaxViewCount)
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline frame '{0}' requested {1} views, but backend '{2}' supports only {3}.", frame.SceneName, frame.Views.size(), capabilities.BackendName, capabilities.MaxViewCount);
        }

        std::unordered_set<std::string> passIds;

        for (const RenderPass& pass : frame.Passes)
        {
            if (pass.Id.empty())
            {
                WAYFINDER_WARNING(LogRenderer, "RenderPipeline skipped an unnamed render pass in frame '{0}'.", frame.SceneName);
                continue;
            }

            if (!passIds.emplace(pass.Id).second)
            {
                WAYFINDER_WARNING(LogRenderer, "RenderPipeline encountered duplicate pass id '{0}' in frame '{1}'.", pass.Id, frame.SceneName);
            }

            ExecutePass(pass, frame, renderAPI, resources);
        }
    }

    void RenderPipeline::ExecutePass(const RenderPass& pass, const RenderFrame& frame, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        const RenderBackendCapabilities& capabilities = renderAPI.GetCapabilities();

        if (!pass.Enabled)
        {
            return;
        }

        if (pass.ViewIndex >= frame.Views.size())
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline skipped pass '{0}' because it referenced missing view index {1} for frame '{2}'.", pass.Id, pass.ViewIndex, frame.SceneName);
            return;
        }

        if (pass.ViewIndex >= capabilities.MaxViewCount)
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline skipped pass '{0}' because backend '{1}' supports only {2} active view(s).", pass.Id, capabilities.BackendName, capabilities.MaxViewCount);
            return;
        }

        const RenderView& view = frame.Views[pass.ViewIndex];

        switch (pass.Kind)
        {
        case RenderPassKind::Scene:
            if (!capabilities.SupportsScenePasses)
            {
                WAYFINDER_WARNING(LogRenderer, "RenderPipeline skipped scene pass '{0}' because backend '{1}' does not support scene passes.", pass.Id, capabilities.BackendName);
                return;
            }
            ExecuteScenePass(pass, view, renderAPI, resources);
            break;
        case RenderPassKind::Debug:
            if (!capabilities.SupportsDebugPasses)
            {
                WAYFINDER_WARNING(LogRenderer, "RenderPipeline skipped debug pass '{0}' because backend '{1}' does not support debug passes.", pass.Id, capabilities.BackendName);
                return;
            }
            ExecuteDebugPass(pass, view, renderAPI);
            break;
        }
    }

    void RenderPipeline::ExecuteScenePass(const RenderPass& pass, const RenderView& view, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        if (!pass.SceneLayer)
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline skipped scene pass '{0}' because it has no scene layer binding.", pass.Id);
            return;
        }

        renderAPI.Begin3DMode(view.CameraState);

        std::vector<const RenderMeshSubmission*> sortedMeshes;
        sortedMeshes.reserve(pass.Meshes.size());
        for (const RenderMeshSubmission& mesh : pass.Meshes)
        {
            if (!mesh.Visible)
            {
                continue;
            }

            sortedMeshes.push_back(&mesh);
        }

        std::stable_sort(sortedMeshes.begin(), sortedMeshes.end(), [](const RenderMeshSubmission* lhs, const RenderMeshSubmission* rhs)
        {
            return std::tie(lhs->SortPriority, lhs->SortKey) < std::tie(rhs->SortPriority, rhs->SortKey);
        });

        for (const RenderMeshSubmission* mesh : sortedMeshes)
        {
            DrawMeshSubmission(*mesh, renderAPI, resources);
        }

        renderAPI.End3DMode();
    }

    void RenderPipeline::ExecuteDebugPass(const RenderPass& pass, const RenderView& view, IRenderAPI& renderAPI) const
    {
        const RenderBackendCapabilities& capabilities = renderAPI.GetCapabilities();

        if (!pass.DebugDraw)
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline skipped debug pass '{0}' because it has no debug payload.", pass.Id);
            return;
        }

        const RenderDebugDrawList& debug = *pass.DebugDraw;
        renderAPI.Begin3DMode(view.CameraState);

        if (debug.ShowWorldGrid)
        {
            renderAPI.DrawGrid(debug.WorldGridSlices, debug.WorldGridSpacing);
        }

        if (!capabilities.SupportsDebugLines && !debug.Lines.empty())
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline skipped {0} debug line(s) in pass '{1}' because backend '{2}' does not support debug lines.", debug.Lines.size(), pass.Id, capabilities.BackendName);
        }

        if (capabilities.SupportsDebugLines)
        {
            for (const RenderDebugLine& debugLine : debug.Lines)
            {
                renderAPI.DrawLine3D(debugLine.Start, debugLine.End, debugLine.Color);
            }
        }

        for (const RenderDebugBox& debugBox : debug.Boxes)
        {
            DrawDebugBox(debugBox, renderAPI);
        }

        renderAPI.End3DMode();
    }

    void RenderPipeline::DrawMeshSubmission(const RenderMeshSubmission& mesh, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        const RenderBackendCapabilities& capabilities = renderAPI.GetCapabilities();
        const RenderMeshResource& resource = resources.ResolveMesh(mesh);

        switch (resource.Geometry.Type)
        {
        case RenderGeometryType::Box:
            if (!capabilities.SupportsBoxGeometry)
            {
                WAYFINDER_WARNING(LogRenderer, "RenderPipeline skipped box submission because backend '{0}' does not support box geometry.", capabilities.BackendName);
                return;
            }
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