#include "RenderPipeline.h"

#include "RenderContext.h"
#include "rendering/backend/GPUPipeline.h"
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
    void RenderPipeline::AddEnginePass(std::unique_ptr<RenderPass> pass)
    {
        const RenderPassContext ctx{*m_context};
        pass->OnAttach(ctx);
        m_enginePasses.push_back(std::move(pass));
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

        AddEnginePass(std::make_unique<SceneOpaquePass>());
        AddEnginePass(std::make_unique<DebugPass>());
    }

    void RenderPipeline::Shutdown()
    {
        if (m_context)
        {
            const RenderPassContext ctx{*m_context};
            for (auto& pass : m_enginePasses)
            {
                pass->OnDetach(ctx);
            }
        }
        m_enginePasses.clear();
        m_context = nullptr;
    }

    bool RenderPipeline::Prepare(RenderFrame& frame, uint32_t swapchainWidth, uint32_t swapchainHeight) const
    {
        if (frame.Views.empty())
        {
            WAYFINDER_WARN(LogRenderer, "RenderPipeline: frame '{}' has no views — skipped", frame.SceneName);
            return false;
        }

        if (frame.Passes.empty())
        {
            WAYFINDER_WARN(LogRenderer, "RenderPipeline: frame '{}' has no passes — skipped", frame.SceneName);
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

        for (FramePass& pass : frame.Passes)
        {
            if (!pass.Enabled || pass.Id.IsEmpty())
            {
                continue;
            }

            if (pass.ViewIndex >= frame.Views.size())
            {
                WAYFINDER_WARN(LogRenderer, "RenderPipeline: pass '{}' references invalid view index {}", pass.Id, pass.ViewIndex);
                pass.Enabled = false;
                continue;
            }

            if (pass.Kind == RenderPassKind::Scene)
            {
                const Frustum& frustum = frame.Views.at(pass.ViewIndex).ViewFrustum;

                std::erase_if(pass.Meshes, [&frustum](const RenderMeshSubmission& submission)
                {
                    if (!submission.Visible)
                    {
                        return true;
                    }
                    return !frustum.TestBounds(submission.WorldSphere, submission.WorldBounds);
                });

                std::ranges::sort(pass.Meshes, [](const RenderMeshSubmission& a, const RenderMeshSubmission& b)
                {
                    return a.SortKey < b.SortKey;
                });
            }
        }

        return true;
    }

    void RenderPipeline::BuildGraph(RenderGraph& graph, const RenderPipelineFrameParams& params, std::span<const std::unique_ptr<RenderPass>> gamePasses) const
    {
        for (const auto& pass : m_enginePasses)
        {
            if (pass->IsEnabled())
            {
                pass->AddPasses(graph, params);
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
            auto colour = graph.FindHandle(WellKnown::SceneColour);
            builder.ReadTexture(colour);
            builder.SetSwapchainOutput(LoadOp::DontCare);

            return [this, colour](RenderDevice& device, const RenderGraphResources& resources)
            {
                auto sceneColourTex = resources.GetTexture(colour);

                const ShaderProgram* compProgram = m_context->GetPrograms().Find("composition");
                const auto nearestSampler = m_context->GetNearestSampler();
                if (!compProgram || !compProgram->Pipeline || !sceneColourTex || !nearestSampler)
                {
                    return;
                }

                compProgram->Pipeline->Bind();
                device.BindFragmentSampler(0, sceneColourTex, nearestSampler);
                device.DrawPrimitives(3);
            };
        });
    }

} // namespace Wayfinder
