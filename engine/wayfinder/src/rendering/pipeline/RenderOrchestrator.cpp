#include "RenderOrchestrator.h"

#include "RenderServices.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/materials/ShaderProgram.h"
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
            for (const auto& desc : feature.GetShaderPrograms())
            {
                if (!registry.Register(desc))
                {
                    WAYFINDER_WARN(LogRenderer, "RenderOrchestrator: failed to register shader program '{}' from feature '{}'", desc.Name, feature.GetName());
                }
            }
        }
    } // namespace

    void RenderOrchestrator::SortFeatureList(std::vector<FeatureSlot>& slots)
    {
        std::ranges::sort(slots, [](const FeatureSlot& a, const FeatureSlot& b)
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

    void RenderOrchestrator::RegisterFeature(const RenderPhase phase, const int32_t order, std::unique_ptr<RenderFeature> feature)
    {
        if (!feature)
        {
            return;
        }

        if (!m_context)
        {
            // Not yet initialised -- defer until Initialise flushes.
            m_pendingFeatures.push_back(FeatureSlot{
                .Phase = phase,
                .Order = order,
                .InsertSequence = m_nextInsertSequence++,
                .Feature = std::move(feature),
            });
            return;
        }

        const RenderFeatureContext ctx{*m_context};
        auto& registry = m_context->GetPrograms();

        // Late-registered feature: register effects if registry not yet sealed.
        if (auto* effectReg = m_context->GetBlendableEffectRegistry())
        {
            feature->OnRegisterEffects(*effectReg);
        }

        // Register shader programs from the feature.
        RegisterFeaturePrograms(*feature, registry);

        feature->OnAttach(ctx);

        FeatureSlot slot{
            .Phase = phase,
            .Order = order,
            .InsertSequence = m_nextInsertSequence++,
            .Feature = std::move(feature),
        };

        if (phase == RenderPhase::Present)
        {
            if (slot.Feature && !slot.Feature->IsEnabled())
            {
                WAYFINDER_WARN(LogRenderer, "RegisterFeature: Present phase feature '{}' is disabled -- graph may lack a swapchain writer", slot.Feature->GetName());
            }
        }

        m_features.push_back(std::move(slot));
        SortFeatureList(m_features);
    }

    void RenderOrchestrator::Initialise(RenderServices& services)
    {
        if (m_initialised)
        {
            return;
        }

        m_context = &services;

        // Flush features that were registered before Initialise (deferred).
        if (!m_pendingFeatures.empty())
        {
            auto deferred = std::move(m_pendingFeatures);
            m_pendingFeatures.clear();
            for (auto& slot : deferred)
            {
                if (slot.Feature)
                {
                    if (slot.Phase == RenderPhase::Present && !slot.Feature->IsEnabled())
                    {
                        WAYFINDER_WARN(LogRenderer, "RegisterFeature: Present phase feature '{}' is disabled -- graph may lack a swapchain writer", slot.Feature->GetName());
                    }
                    m_features.push_back(std::move(slot));
                }
            }
            SortFeatureList(m_features);
        }

        // 1. Register blendable effects (before seal).
        if (auto* effectReg = m_context->GetBlendableEffectRegistry())
        {
            for (auto& slot : m_features)
            {
                if (slot.Feature)
                {
                    slot.Feature->OnRegisterEffects(*effectReg);
                }
            }
        }

        // 2. Collect and register shader programs from all features.
        auto& programs = m_context->GetPrograms();
        for (auto& slot : m_features)
        {
            if (slot.Feature)
            {
                RegisterFeaturePrograms(*slot.Feature, programs);
            }
        }

        // 3. OnAttach for runtime init.
        const RenderFeatureContext ctx{*m_context};
        for (auto& slot : m_features)
        {
            if (slot.Feature)
            {
                slot.Feature->OnAttach(ctx);
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
        for (auto& slot : m_features)
        {
            if (slot.Feature)
            {
                RegisterFeaturePrograms(*slot.Feature, programs);
            }
        }

        WAYFINDER_INFO(LogRenderer, "RenderOrchestrator::RebuildPipelines: shader programs and pipelines re-registered");
    }

    void RenderOrchestrator::Shutdown()
    {
        if (m_context)
        {
            const RenderFeatureContext ctx{*m_context};
            for (auto& slot : m_features)
            {
                if (slot.Feature)
                {
                    slot.Feature->OnDetach(ctx);
                }
            }
        }
        m_features.clear();
        m_pendingFeatures.clear();
        m_nextInsertSequence = 0;
        m_context = nullptr;
        m_initialised = false;
    }

    void RenderOrchestrator::BuildGraph(RenderGraph& graph, const FrameRenderParams& params) const
    {
        for (const auto& slot : m_features)
        {
            if (slot.Feature && slot.Feature->IsEnabled())
            {
                slot.Feature->AddPasses(graph, params);
            }
        }
    }

} // namespace Wayfinder
