#include "RenderOrchestrator.h"

#include "RenderServices.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/passes/ChromaticAberrationFeature.h"
#include "rendering/passes/ColourGradingFeature.h"
#include "rendering/passes/CompositionPass.h"
#include "rendering/passes/DebugPass.h"
#include "rendering/passes/SceneOpaquePass.h"
#include "rendering/passes/VignetteFeature.h"
#include "volumes/BlendableEffectRegistry.h"

#include "core/Log.h"
#include "maths/Frustum.h"
#include "maths/Maths.h"

#include <algorithm>
#include <vector>

namespace Wayfinder
{
    namespace
    {
        /// Collects shader programs from a feature and registers them with the registry.
        void RegisterFeaturePrograms(RenderFeature& feature, ShaderProgramRegistry& registry)
        {
            for (auto& desc : feature.GetShaderPrograms())
            {
                if (!registry.Register(desc))
                {
                    WAYFINDER_WARN(LogRenderer, "RenderOrchestrator: failed to register shader program '{}' from feature '{}'", desc.Name, feature.GetName());
                }
            }
        }
    } // namespace

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
        auto& registry = m_context->GetPrograms();

        // Late-registered feature: register effects if registry not yet sealed.
        if (auto* effectReg = m_context->GetBlendableEffectRegistry())
        {
            pass->OnRegisterEffects(*effectReg);
        }

        // Register shader programs from the feature.
        RegisterFeaturePrograms(*pass, registry);

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

        // Create built-in features. Queue directly rather than through RegisterPass
        // so the batch lifecycle below handles effects, programs, and attach in order.
        // @todo: this shouldn't be done in here, we should be able to compose this through TOML/JSON or something else without hardcoding built-in features in the orchestrator.
        auto addBuiltIn = [&](const RenderPhase phase, const int32_t order, std::unique_ptr<RenderFeature> pass)
        {
            m_passes.push_back(PassSlot{
                .Phase = phase,
                .Order = order,
                .InsertSequence = m_nextPassInsertSequence++,
                .Pass = std::move(pass),
            });
        };

        addBuiltIn(RenderPhase::Opaque, 0, std::make_unique<SceneOpaquePass>());
        addBuiltIn(RenderPhase::PostProcess, 800, std::make_unique<ChromaticAberrationFeature>());
        addBuiltIn(RenderPhase::PostProcess, 900, std::make_unique<Rendering::VignetteFeature>());
        addBuiltIn(RenderPhase::Composite, 0, std::make_unique<ColourGradingFeature>());
        addBuiltIn(RenderPhase::Overlay, 0, std::make_unique<DebugPass>());
        addBuiltIn(RenderPhase::Present, 0, std::make_unique<CompositionPass>());

        // Flush passes that were registered before Initialise (deferred).
        if (!m_pendingPasses.empty())
        {
            auto deferred = std::move(m_pendingPasses);
            m_pendingPasses.clear();
            for (auto& slot : deferred)
            {
                if (slot.Pass)
                {
                    if (slot.Phase == RenderPhase::Present && !slot.Pass->IsEnabled())
                    {
                        WAYFINDER_WARN(LogRenderer, "RegisterPass: Present phase pass '{}' is disabled -- graph may lack a swapchain writer", slot.Pass->GetName());
                    }
                    m_passes.push_back(std::move(slot));
                }
            }
            SortPassList(m_passes);
        }

        // 1. Register blendable effects (before seal).
        if (auto* effectReg = m_context->GetBlendableEffectRegistry())
        {
            for (auto& slot : m_passes)
            {
                if (slot.Pass)
                {
                    slot.Pass->OnRegisterEffects(*effectReg);
                }
            }
        }

        // 2. Collect and register shader programs from all features.
        auto& programs = m_context->GetPrograms();
        for (auto& slot : m_passes)
        {
            if (slot.Pass)
            {
                RegisterFeaturePrograms(*slot.Pass, programs);
            }
        }

        // 3. OnAttach for runtime init.
        const RenderFeatureContext ctx{*m_context};
        for (auto& slot : m_passes)
        {
            if (slot.Pass)
            {
                slot.Pass->OnAttach(ctx);
            }
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

        // Collect and re-register shader programs from all features.
        auto& programs = m_context->GetPrograms();
        for (auto& slot : m_passes)
        {
            if (slot.Pass)
            {
                RegisterFeaturePrograms(*slot.Pass, programs);
            }
        }

        WAYFINDER_INFO(LogRenderer, "RenderOrchestrator::RebuildPipelines: shader programs and pipelines re-registered");
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
