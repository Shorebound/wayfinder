#include "RenderPipeline.h"

#include "ForwardOpaqueShaderPrograms.h"
#include "RenderContext.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/passes/DebugPass.h"
#include "rendering/passes/SceneOpaquePass.h"

#include "core/Log.h"
#include "maths/Frustum.h"
#include "maths/Maths.h"

#include <algorithm>
#include <vector>

namespace Wayfinder
{
    void RenderPipeline::SortEnginePasses()
    {
        std::ranges::sort(m_enginePasses, [](const EnginePassSlot& a, const EnginePassSlot& b)
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

    void RenderPipeline::RegisterEnginePass(EngineRenderPhase phase, int32_t orderWithinPhase, std::unique_ptr<RenderPass> pass)
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

        m_enginePasses.push_back(EnginePassSlot{
            .Phase = phase,
            .OrderWithinPhase = orderWithinPhase,
            .InsertSequence = m_nextEnginePassInsertSequence++,
            .Pass = std::move(pass),
        });
        SortEnginePasses();
    }

    void RenderPipeline::Initialise(RenderContext& context)
    {
        m_context = &context;

        auto& registry = context.GetPrograms();

        {
            ShaderProgramDesc desc;
            desc.Name = "composition";
            desc.VertexShaderName = "fullscreen";
            desc.FragmentShaderName = "composition";
            desc.VertexResources = {};
            desc.FragmentResources = {.numUniformBuffers = 0, .numSamplers = 1};
            desc.VertexLayout = VertexLayouts::Empty;
            desc.Cull = CullMode::None;
            desc.DepthTest = false;
            desc.DepthWrite = false;
            desc.MaterialUBOSize = 0;
            desc.VertexUBOSize = 0;
            desc.NeedsSceneGlobals = false;

            registry.Register(desc);
        }

        RegisterForwardOpaquePrograms(registry);

        RegisterEnginePass(EngineRenderPhase::OpaqueMain, 0, std::make_unique<SceneOpaquePass>());
        RegisterEnginePass(EngineRenderPhase::Debug, 0, std::make_unique<DebugPass>());
    }

    void RenderPipeline::Shutdown()
    {
        if (m_context)
        {
            const RenderPassContext ctx{*m_context};
            for (auto& slot : m_enginePasses)
            {
                if (slot.Pass)
                {
                    slot.Pass->OnDetach(ctx);
                }
            }
        }
        m_enginePasses.clear();
        m_nextEnginePassInsertSequence = 0;
        m_context = nullptr;
    }

    bool RenderPipeline::Prepare(RenderFrame& frame, uint32_t swapchainWidth, uint32_t swapchainHeight) const
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

    void RenderPipeline::BuildGraph(RenderGraph& graph, const RenderPipelineFrameParams& params, std::span<const std::unique_ptr<RenderPass>> gamePasses) const
    {
        for (const auto& slot : m_enginePasses)
        {
            if (slot.Pass && slot.Pass->IsEnabled())
            {
                slot.Pass->AddPasses(graph, params);
            }
        }

        for (const auto& pass : gamePasses)
        {
            if (pass->IsEnabled())
            {
                pass->AddPasses(graph, params);
            }
        }

        graph.AddPass("Composition", [&](RenderGraphBuilder& builder)
        {
            auto colour = graph.FindHandleChecked(WellKnown::SceneColour);
            builder.ReadTexture(colour);
            builder.SetSwapchainOutput(LoadOp::DontCare);

            return [this, colour](RenderDevice& device, const RenderGraphResources& resources)
            {
                auto sceneColourTex = resources.GetTexture(colour);

                const ShaderProgram* compProgram = m_context->GetPrograms().Find("composition");
                const auto nearestSampler = m_context->GetNearestSampler();
                if (!compProgram || !compProgram->Pipeline || !sceneColourTex || !nearestSampler)
                {
#if defined(NDEBUG)
                    WAYFINDER_WARN(LogRenderer, "Composition: missing program, pipeline, scene colour, or sampler — skipped");
#else
                    WAYFINDER_ERROR(LogRenderer, "Composition: missing program, pipeline, scene colour, or sampler — skipped");
#endif
                    return;
                }

                compProgram->Pipeline->Bind();
                device.BindFragmentSampler(0, sceneColourTex, nearestSampler);
                device.DrawPrimitives(3);
            };
        });
    }

} // namespace Wayfinder
