#include "RenderOrchestrator.h"

#include "RenderServices.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/passes/ChromaticAberrationFeature.h"
#include "rendering/passes/ColourGradingFeature.h"
#include "rendering/passes/CompositionPass.h"
#include "rendering/passes/DebugPass.h"
#include "rendering/passes/SceneOpaquePass.h"
#include "rendering/passes/VignetteFeature.h"

#include "core/Log.h"
#include "maths/Frustum.h"
#include "maths/Maths.h"

#include <algorithm>
#include <vector>

namespace Wayfinder
{
    void RenderOrchestrator::SortPassList(std::vector<PassSlot>& slots)
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

    void RenderOrchestrator::RegisterPass(const RenderPhase phase, const int32_t order, std::unique_ptr<RenderFeature> pass)
    {
        if (!pass)
        {
            return;
        }

        if (!m_context)
        {
            // Not yet initialised — defer until Initialise flushes.
            m_pendingPasses.push_back(PassSlot{
                .Phase = phase,
                .Order = order,
                .InsertSequence = m_nextPassInsertSequence++,
                .Pass = std::move(pass),
            });
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

    void RenderOrchestrator::Initialise(RenderServices& services)
    {
        if (m_initialised)
        {
            return;
        }

        m_context = &services;

        if (!RegisterSceneShaderPrograms(services.GetPrograms()))
        {
            // Headless/Null backends often lack .spv assets; passes still attach so ordering/graph tests can run.
            WAYFINDER_ERROR(LogRenderer, "RenderOrchestrator: one or more scene shader programs failed to register — pipeline may be incomplete");
        }

        RegisterPass(RenderPhase::Opaque, 0, std::make_unique<SceneOpaquePass>());
        RegisterPass(RenderPhase::PostProcess, 800, std::make_unique<ChromaticAberrationFeature>());
        RegisterPass(RenderPhase::PostProcess, 900, std::make_unique<Rendering::VignetteFeature>());
        RegisterPass(RenderPhase::Composite, 0, std::make_unique<ColourGradingFeature>());
        RegisterPass(RenderPhase::Overlay, 0, std::make_unique<DebugPass>());
        RegisterPass(RenderPhase::Present, 0, std::make_unique<CompositionPass>());

        // Flush passes that were registered before Initialise (deferred).
        // Insert directly with their captured InsertSequence to preserve original
        // call order for equal Phase/Order entries (RegisterPass would reassign it).
        if (!m_pendingPasses.empty())
        {
            auto deferred = std::move(m_pendingPasses);
            m_pendingPasses.clear();

            const RenderFeatureContext ctx{*m_context};
            for (auto& slot : deferred)
            {
                if (slot.Pass)
                {
                    slot.Pass->OnAttach(ctx);

                    if (slot.Phase == RenderPhase::Present && !slot.Pass->IsEnabled())
                    {
                        WAYFINDER_WARN(LogRenderer, "RegisterPass: Present phase pass '{}' is disabled — graph may lack a swapchain writer", slot.Pass->GetName());
                    }

                    m_passes.push_back(std::move(slot));
                }
            }
            SortPassList(m_passes);
        }

        m_initialised = true;
    }

    void RenderOrchestrator::RebuildPipelines()
    {
        if (!m_context || !m_initialised)
        {
            WAYFINDER_WARN(LogRenderer, "RenderOrchestrator::RebuildPipelines called before Initialise");
            return;
        }

        if (!RegisterSceneShaderPrograms(m_context->GetPrograms()))
        {
            WAYFINDER_ERROR(LogRenderer, "RenderOrchestrator::RebuildPipelines: one or more shader programs failed to re-register");
        }
        else
        {
            WAYFINDER_INFO(LogRenderer, "RenderOrchestrator::RebuildPipelines: shader programs re-registered");
        }
    }

    void RenderOrchestrator::Shutdown()
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
        m_pendingPasses.clear();
        m_nextPassInsertSequence = 0;
        m_context = nullptr;
        m_initialised = false;
    }

    bool RenderOrchestrator::Prepare(RenderFrame& frame, const uint32_t swapchainWidth, const uint32_t swapchainHeight) const
    {
        if (frame.Views.empty())
        {
            WAYFINDER_WARN(LogRenderer, "RenderOrchestrator: frame '{}' has no views — skipped", frame.SceneName);
            return false;
        }

        if (frame.Layers.empty())
        {
            WAYFINDER_WARN(LogRenderer, "RenderOrchestrator: frame '{}' has no layers — skipped", frame.SceneName);
            return false;
        }

        if (swapchainWidth == 0 || swapchainHeight == 0)
        {
            WAYFINDER_WARN(LogRenderer, "RenderOrchestrator: swapchain extent is zero — skipping frame '{}'", frame.SceneName);
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
                WAYFINDER_WARN(LogRenderer, "RenderOrchestrator: layer '{}' references invalid view index {}", layer.Id, layer.ViewIndex);
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

    void RenderOrchestrator::BuildGraph(RenderGraph& graph, const FrameRenderParams& params) const
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
