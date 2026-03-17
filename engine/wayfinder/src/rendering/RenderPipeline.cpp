#include "RenderPipeline.h"

#include "RenderAPI.h"

#include <algorithm>

namespace Wayfinder
{
    void RenderPipeline::Execute(const RenderFrame& frame, const Camera& camera, IRenderAPI& renderAPI) const
    {
        ExecuteScenePass(frame, camera, renderAPI);
        ExecuteDebugPass(frame, camera, renderAPI);
    }

    void RenderPipeline::ExecuteScenePass(const RenderFrame& frame, const Camera& camera, IRenderAPI& renderAPI) const
    {
        renderAPI.Begin3DMode(camera);
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
            DrawMeshSubmission(*mesh, renderAPI);
        }

        renderAPI.End3DMode();
    }

    void RenderPipeline::ExecuteDebugPass(const RenderFrame& frame, const Camera& camera, IRenderAPI& renderAPI) const
    {
        renderAPI.Begin3DMode(camera);

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

    void RenderPipeline::DrawMeshSubmission(const RenderMeshSubmission& mesh, IRenderAPI& renderAPI) const
    {
        switch (mesh.Geometry.Type)
        {
        case RenderGeometryType::Box:
            ApplyMaterialBinding(mesh.Material, mesh.LocalToWorld, mesh.Geometry.Dimensions, renderAPI);
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