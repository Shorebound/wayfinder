#include "RenderPipeline.h"

#include "ForwardOpaqueShaderPrograms.h"
#include "RenderServices.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/passes/CompositionPass.h"
#include "rendering/passes/DebugPass.h"
#include "rendering/passes/SceneOpaquePass.h"

#include "core/Log.h"
#include "maths/Frustum.h"
#include "maths/Maths.h"

#include <algorithm>
#include <vector>

namespace Wayfinder
{
    void RenderPipeline::SortPassList(std::vector<PassSlot>& slots)
    {
        std::ranges::sort(slots, [](const PassSlot& a, const PassSlot& b)
        {
            if (a.Phase != b.Phase)
            {
                return static_cast<uint8_t>(a.Phase) < static_cast<uint8_t>(b.Phase);
            }
            if (a.Order != b.Order)
            {
                return a.Order < b.Order;
            }
            return a.InsertSequence < b.InsertSequence;
        });
    }

    void RenderPipeline::RegisterPass(const RenderPhase phase, const int32_t order, std::unique_ptr<RenderFeature> pass)
    {
        if (!m_context)
        {
            WAYFINDER_ERROR(LogRenderer, "RegisterPass: pipeline has no context — call Initialise first");
            return;
        }
        if (!pass)
        {
            return;
        }

        const RenderFeatureContext ctx{*m_context};
        pass->OnAttach(ctx);

        PassSlot slot{
            .Phase = phase,
            .Order = order,
            .InsertSequence = m_nextPassInsertSequence++,
            .Pass = std::move(pass),
        };

        if (phase == RenderPhase::Present)
        {
            if (slot.Pass && !slot.Pass->IsEnabled())
            {
                WAYFINDER_WARN(LogRenderer, "RegisterPass: Present phase pass '{}' is disabled — graph may lack a swapchain writer", slot.Pass->GetName());
            }
        }

        m_passes.push_back(std::move(slot));
        SortPassList(m_passes);
    }

    void RenderPipeline::Initialise(RenderServices& services)
    {
        if (m_initialised)
        {
            return;
        }

        m_context = &services;

        services.RegisterEngineBlendableEffects();

        RegisterForwardOpaquePrograms(services.GetPrograms());

        RegisterPass(RenderPhase::Opaque, 0, std::make_unique<SceneOpaquePass>());
        RegisterPass(RenderPhase::Overlay, 0, std::make_unique<DebugPass>());
        RegisterPass(RenderPhase::Present, 0, std::make_unique<CompositionPass>());

        m_initialised = true;
    }

    void RenderPipeline::Shutdown()
    {
        if (m_context)
        {
            const RenderFeatureContext ctx{*m_context};
            for (auto& slot : m_passes)
            {
                if (slot.Pass)
                {
                    slot.Pass->OnDetach(ctx);
                }
            }
        }
        m_passes.clear();
        m_nextPassInsertSequence = 0;
        m_context = nullptr;
        m_initialised = false;
    }

    bool RenderPipeline::Prepare(RenderFrame& frame, const uint32_t swapchainWidth, const uint32_t swapchainHeight) const
    {
        if (frame.Views.empty())
        {
            WAYFINDER_WARN(LogRenderer, "RenderPipeline: frame '{}' has no views — skipped", frame.SceneName);
            return false;
        }

        if (frame.Layers.empty())
        {
            WAYFINDER_WARN(LogRenderer, "RenderPipeline: frame '{}' has no layers — skipped", frame.SceneName);
            return false;
        }

        const float aspect = (swapchainHeight > 0) ? static_cast<float>(swapchainWidth) / static_cast<float>(swapchainHeight) : 1.0f;

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
                WAYFINDER_WARN(LogRenderer, "RenderPipeline: layer '{}' references invalid view index {}", layer.Id, layer.ViewIndex);
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

    void RenderPipeline::BuildGraph(RenderGraph& graph, const RenderPipelineFrameParams& params) const
    {
        for (const auto& slot : m_passes)
        {
            if (slot.Pass && slot.Pass->IsEnabled())
            {
                slot.Pass->AddPasses(graph, params);
            }
        }
    }

} // namespace Wayfinder
