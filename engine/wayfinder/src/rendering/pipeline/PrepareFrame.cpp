#include "PrepareFrame.h"

#include "rendering/graph/RenderFrame.h"

#include "core/Log.h"
#include "maths/Frustum.h"
#include "maths/Maths.h"

namespace Wayfinder::Rendering
{
    bool PrepareFrame(RenderFrame& frame, const uint32_t swapchainWidth, const uint32_t swapchainHeight)
    {
        if (frame.Views.empty())
        {
            WAYFINDER_WARN(LogRenderer, "PrepareFrame: frame '{}' has no views -- skipped", frame.SceneName);
            return false;
        }

        if (frame.Layers.empty())
        {
            WAYFINDER_WARN(LogRenderer, "PrepareFrame: frame '{}' has no layers -- skipped", frame.SceneName);
            return false;
        }

        if (swapchainWidth == 0 || swapchainHeight == 0)
        {
            WAYFINDER_WARN(LogRenderer, "PrepareFrame: swapchain extent is zero -- skipping frame '{}'", frame.SceneName);
            return false;
        }

        const float aspect = static_cast<float>(swapchainWidth) / static_cast<float>(swapchainHeight);

        for (RenderView& view : frame.Views)
        {
            const auto& cam = view.CameraState;
            view.ViewMatrix = Maths::LookAt(cam.Position, cam.Target, cam.Up);

            if (cam.ProjectionType == 0)
            {
                view.ProjectionMatrix = Maths::PerspectiveRH_ZO(Maths::ToRadians(cam.FOV), aspect, cam.NearPlane, cam.FarPlane);
            }
            else
            {
                const float halfH = cam.FOV * 0.5f;
                const float halfW = halfH * aspect;
                view.ProjectionMatrix = Maths::OrthoRH_ZO(-halfW, halfW, -halfH, halfH, cam.NearPlane, cam.FarPlane);
            }

            view.ViewFrustum = Frustum::ExtractPlanes(view.ProjectionMatrix * view.ViewMatrix);
            view.Prepared = true;
        }

        for (FrameLayer& layer : frame.Layers)
        {
            if (!layer.Enabled || layer.Id.IsEmpty())
            {
                continue;
            }

            if (layer.ViewIndex >= frame.Views.size())
            {
                WAYFINDER_WARN(LogRenderer, "PrepareFrame: layer '{}' references invalid view index {}", layer.Id, layer.ViewIndex);
                layer.Enabled = false;
                continue;
            }

            if (layer.Kind == FrameLayerKind::Scene)
            {
                const Frustum& frustum = frame.Views.at(layer.ViewIndex).ViewFrustum;

                std::erase_if(layer.Meshes, [&frustum](const RenderMeshSubmission& submission)
                {
                    if (!submission.Visible)
                    {
                        return true;
                    }
                    return !frustum.TestBounds(submission.WorldSphere, submission.WorldBounds);
                });

                std::ranges::sort(layer.Meshes, [](const RenderMeshSubmission& a, const RenderMeshSubmission& b)
                {
                    return a.SortKey < b.SortKey;
                });
            }
        }

        return true;
    }

} // namespace Wayfinder::Rendering
