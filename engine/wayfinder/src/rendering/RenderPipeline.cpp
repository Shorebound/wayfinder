#include "RenderPipeline.h"

#include "RenderAPI.h"
#include "RenderResources.h"

#include <algorithm>

namespace Wayfinder
{
    void RenderPipeline::Execute(const RenderFrame& frame, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        if (frame.Views.empty())
        {
            return;
        }

        for (const RenderView& view : frame.Views)
        {
            ExecuteView(frame, view, renderAPI, resources);
        }
    }

    void RenderPipeline::ExecuteView(const RenderFrame& frame, const RenderView& view, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        ExecuteScenePass(frame, view, renderAPI, resources);
        ExecuteDebugPass(frame, view, renderAPI, resources);
    }

    void RenderPipeline::ExecuteScenePass(const RenderFrame& frame, const RenderView& view, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        renderAPI.Begin3DMode(view.CameraState);
        renderAPI.DrawGrid(100, 1.0f);

        std::vector<const RenderMeshSubmission*> sortedMeshes;
        sortedMeshes.reserve(frame.Meshes.size());
        for (const RenderMeshSubmission& mesh : frame.Meshes)
        {
            sortedMeshes.push_back(&mesh);
        }

        std::stable_sort(sortedMeshes.begin(), sortedMeshes.end(), [](const RenderMeshSubmission* lhs, const RenderMeshSubmission* rhs)
        {
            return lhs->SortKey < rhs->SortKey;
        });

        for (const RenderMeshSubmission* mesh : sortedMeshes)
        {
            DrawMeshSubmission(*mesh, renderAPI, resources);
        }

        renderAPI.End3DMode();
    }

    void RenderPipeline::ExecuteDebugPass(const RenderFrame& frame, const RenderView& view, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        renderAPI.Begin3DMode(view.CameraState);

        for (const RenderDebugLine& debugLine : frame.Debug.Lines)
        {
            renderAPI.DrawLine3D(debugLine.Start, debugLine.End, debugLine.Color);
        }

        for (const RenderDebugBox& debugBox : frame.Debug.Boxes)
        {
            DrawDebugBox(debugBox, renderAPI, resources);
        }

        renderAPI.End3DMode();
    }

    void RenderPipeline::DrawMeshSubmission(const RenderMeshSubmission& mesh, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        const RenderMeshResource& resource = resources.ResolveMesh(mesh);
        const RenderMaterialBinding material = resources.ResolveMaterialBinding(mesh.Material);

        switch (resource.Geometry.Type)
        {
        case RenderGeometryType::Box:
            ApplyMaterialBinding(material, mesh.LocalToWorld, resource.Geometry.Dimensions, renderAPI);
            break;
        }
    }

    void RenderPipeline::DrawDebugBox(const RenderDebugBox& debugBox, IRenderAPI& renderAPI, RenderResourceCache& resources) const
    {
        const RenderMaterialBinding material = resources.ResolveMaterialBinding(debugBox.Material);
        ApplyMaterialBinding(material, debugBox.LocalToWorld, debugBox.Dimensions, renderAPI);
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