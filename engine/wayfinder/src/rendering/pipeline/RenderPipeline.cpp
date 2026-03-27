#include "RenderPipeline.h"

#include "ForwardOpaqueShaderPrograms.h"
#include "RenderContext.h"
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
    void RenderPipeline::SortEnginePassList(std::vector<EnginePassSlot>& slots)
    {
        std::ranges::sort(slots, [](const EnginePassSlot& a, const EnginePassSlot& b)
        {
            if (a.Phase != b.Phase)
            {
                return static_cast<uint8_t>(a.Phase) < static_cast<uint8_t>(b.Phase);
            }
            if (a.OrderWithinPhase != b.OrderWithinPhase)
            {
                return a.OrderWithinPhase < b.OrderWithinPhase;
            }
            return a.InsertSequence < b.InsertSequence;
        });
    }

    void RenderPipeline::RegisterEnginePass(const EngineRenderPhase phase, const int32_t orderWithinPhase, std::unique_ptr<RenderPass> pass)
    {
        if (!m_context)
        {
            WAYFINDER_ERROR(LogRenderer, "RegisterEnginePass: pipeline has no context — call Initialise first");
            return;
        }
        if (!pass)
        {
            return;
        }

        const RenderPassContext ctx{*m_context};
        pass->OnAttach(ctx);

        EnginePassSlot slot{
            .Phase = phase,
            .OrderWithinPhase = orderWithinPhase,
            .InsertSequence = m_nextEnginePassInsertSequence++,
            .Pass = std::move(pass),
        };

        if (phase == EngineRenderPhase::LateEngine)
        {
            m_lateEnginePasses.push_back(std::move(slot));
            SortEnginePassList(m_lateEnginePasses);
        }
        else
        {
            m_earlyEnginePasses.push_back(std::move(slot));
            SortEnginePassList(m_earlyEnginePasses);
        }
    }

    void RenderPipeline::InvokePassList(RenderGraph& graph, const RenderPipelineFrameParams& params, const std::vector<EnginePassSlot>& slots) const
    {
        for (const auto& slot : slots)
        {
            if (slot.Pass && slot.Pass->IsEnabled())
            {
                slot.Pass->AddPasses(graph, params);
            }
        }
    }

    void RenderPipeline::Initialise(RenderContext& context)
    {
        if (m_initialised)
        {
            return;
        }

        m_context = &context;

        RegisterForwardOpaquePrograms(context.GetPrograms());

        RegisterEnginePass(EngineRenderPhase::OpaqueMain, 0, std::make_unique<SceneOpaquePass>());
        RegisterEnginePass(EngineRenderPhase::Debug, 0, std::make_unique<DebugPass>());
        // PresentSourceCopyPass is optional: register `RenderPipeline::RegisterEnginePass(LateEngine, 0, PresentSourceCopyPass)`
        // when a game needs a stable PresentSource handoff. Omitting it lets Composition sample SceneColour directly and
        // avoids an undefined PresentSource texture when the copy draw cannot run.
        RegisterEnginePass(EngineRenderPhase::LateEngine, 0, std::make_unique<CompositionPass>());

        m_initialised = true;
    }

    void RenderPipeline::Shutdown()
    {
        if (m_context)
        {
            const RenderPassContext ctx{*m_context};
            for (auto& slot : m_earlyEnginePasses)
            {
                if (slot.Pass)
                {
                    slot.Pass->OnDetach(ctx);
                }
            }
            for (auto& slot : m_lateEnginePasses)
            {
                if (slot.Pass)
                {
                    slot.Pass->OnDetach(ctx);
                }
            }
        }
        m_earlyEnginePasses.clear();
        m_lateEnginePasses.clear();
        m_nextEnginePassInsertSequence = 0;
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

        for (FrameLayerRecord& layer : frame.Layers)
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

    void RenderPipeline::BuildGraph(RenderGraph& graph, const RenderPipelineFrameParams& params, const std::span<const std::unique_ptr<RenderPass>> gamePasses) const
    {
        InvokePassList(graph, params, m_earlyEnginePasses);

        for (const auto& pass : gamePasses)
        {
            if (pass && pass->IsEnabled())
            {
                pass->AddPasses(graph, params);
            }
        }

        InvokePassList(graph, params, m_lateEnginePasses);
    }

} // namespace Wayfinder
