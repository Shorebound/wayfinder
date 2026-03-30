#include "SceneOpaquePass.h"

#include "rendering/passes/SubmissionDrawing.h"

#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderCapabilities.h"
#include "rendering/graph/RenderFrameUtils.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/materials/MaterialParameter.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/pipeline/BuiltInUBOs.h"
#include "rendering/pipeline/RenderServices.h"

#include "core/Log.h"
#include "maths/Maths.h"

#ifdef WAYFINDER_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // lambda closure padded due to captured alignas(16) matrices
#endif

namespace Wayfinder
{
    namespace
    {
        /**
         * @prototype Build scene globals from frame lights.
         * Falls back to a hardcoded default directional light when no light is submitted.
         * Should be replaced by data-driven scene defaults (e.g. from scene config or environment settings).
         */
        SceneGlobalsUBO BuildSceneGlobals(const RenderFrame& frame)
        {
            SceneGlobalsUBO globals;

            for (const auto& light : frame.Lights)
            {
                if (light.Type == RenderLightType::Directional)
                {
                    globals.LightDirection = Maths::Normalize(light.Direction);
                    globals.LightIntensity = light.Intensity;
                    globals.LightColour = LinearColour::FromColour(light.Tint).ToFloat3();
                    return globals;
                }
            }

            /// @prototype Hardcoded fallback light direction -- replace with data-driven scene defaults.
            globals.LightDirection = Maths::Normalize(Float3{-0.4f, -0.7f, -0.5f});
            return globals;
        }
    } // namespace

    std::span<const ShaderProgramDesc> SceneOpaquePass::GetShaderPrograms() const
    {
        static const auto programs = []
        {
            std::vector<ShaderProgramDesc> p;
            p.reserve(4);

            {
                ShaderProgramDesc desc;
                desc.Name = "unlit";
                desc.VertexShaderName = "unlit";
                desc.FragmentShaderName = "unlit";
                desc.VertexResources = {.numUniformBuffers = 1};
                desc.FragmentResources = {.numUniformBuffers = 1};
                desc.VertexLayout = VertexLayouts::PosNormalColour;
                desc.Cull = CullMode::Back;
                desc.DepthTest = true;
                desc.DepthWrite = true;
                desc.MaterialParams =
                {
                    {.Name = "base_colour", .Type = MaterialParamType::Colour, .Offset = 0, .Default = LinearColour::White()},
                };
                desc.VertexUBOSize = sizeof(UnlitTransformUBO);
                desc.NeedsSceneGlobals = false;
                p.push_back(std::move(desc));
            }

            {
                ShaderProgramDesc desc;
                desc.Name = "unlit_blended";
                desc.VertexShaderName = "unlit";
                desc.FragmentShaderName = "unlit";
                desc.VertexResources = {.numUniformBuffers = 1};
                desc.FragmentResources = {.numUniformBuffers = 1};
                desc.VertexLayout = VertexLayouts::PosNormalColour;
                desc.Cull = CullMode::Back;
                desc.DepthTest = true;
                desc.DepthWrite = false;
                desc.Blend = BlendPresets::AlphaBlend();
                desc.MaterialParams =
                {
                    {.Name = "base_colour", .Type = MaterialParamType::Colour, .Offset = 0, .Default = LinearColour::White()},
                };
                desc.VertexUBOSize = sizeof(UnlitTransformUBO);
                desc.NeedsSceneGlobals = false;
                p.push_back(std::move(desc));
            }

            {
                ShaderProgramDesc desc;
                desc.Name = "basic_lit";
                desc.VertexShaderName = "basic_lit";
                desc.FragmentShaderName = "basic_lit";
                desc.VertexResources = {.numUniformBuffers = 1};
                desc.FragmentResources = {.numUniformBuffers = 2};
                desc.VertexLayout = VertexLayouts::PosNormalColour;
                desc.Cull = CullMode::Back;
                desc.DepthTest = true;
                desc.DepthWrite = true;
                desc.MaterialParams =
                {
                    {.Name = "base_colour", .Type = MaterialParamType::Colour, .Offset = 0, .Default = LinearColour::White()},
                };
                desc.VertexUBOSize = sizeof(TransformUBO);
                desc.NeedsSceneGlobals = true;
                p.push_back(std::move(desc));
            }

            {
                ShaderProgramDesc desc;
                desc.Name = "textured_lit";
                desc.VertexShaderName = "textured_lit";
                desc.FragmentShaderName = "textured_lit";
                desc.VertexResources = {.numUniformBuffers = 1};
                desc.FragmentResources = {.numUniformBuffers = 2, .numSamplers = 1};
                desc.VertexLayout = VertexLayouts::PosNormalUVTangent;
                desc.Cull = CullMode::Back;
                desc.DepthTest = true;
                desc.DepthWrite = true;
                desc.MaterialParams =
                {
                    {.Name = "base_colour", .Type = MaterialParamType::Colour, .Offset = 0, .Default = LinearColour::White()},
                };
                desc.VertexUBOSize = sizeof(TransformUBO);
                desc.NeedsSceneGlobals = true;
                desc.TextureSlots = {{.Name = "diffuse", .BindingSlot = 0}};
                p.push_back(std::move(desc));
            }

            return p;
        }();

        return programs;
    }

    void SceneOpaquePass::OnAttach(const RenderFeatureContext& context)
    {
        m_context = &context.Context;
    }

    void SceneOpaquePass::AddPasses(RenderGraph& graph, const FrameRenderParams& params)
    {
        if (!m_context)
        {
            WAYFINDER_WARN(LogRenderer, "SceneOpaquePass: no context! Skipped");
            return;
        }

        const auto& preparedFrame = params.Frame;
        const uint32_t swapW = params.SwapchainWidth;
        const uint32_t swapH = params.SwapchainHeight;

        const auto& primary = params.PrimaryView;
        const Colour clearColour = primary.ClearColour;
        const bool hasCamera = primary.Valid;

        const SceneGlobalsUBO sceneGlobals = BuildSceneGlobals(preparedFrame);

        RenderGraphTextureDesc colourDesc;
        colourDesc.Width = swapW;
        colourDesc.Height = swapH;
        /// @todo Verify all downstream readers of SceneColour (composition, post-process
        /// features) handle RGBA16_FLOAT correctly — some blit/tonemap shaders may
        /// assume RGBA8 range or precision.
        colourDesc.Format = TextureFormat::RGBA16_FLOAT;
        colourDesc.DebugName = GraphTextureName(GraphTextureId::SceneColour);

        RenderGraphTextureDesc depthDesc;
        depthDesc.Width = swapW;
        depthDesc.Height = swapH;
        depthDesc.Format = TextureFormat::D32_FLOAT;
        depthDesc.DebugName = GraphTextureName(GraphTextureId::SceneDepth);

        graph.AddPass("MainScene", [&, hasCamera](RenderGraphBuilder& builder)
        {
            builder.DeclarePassCapabilities(RenderCapabilities::RASTER | RenderCapabilities::RASTER_SCENE_GEOMETRY);
            auto colour = builder.CreateTransient(colourDesc);
            auto depth = builder.CreateTransient(depthDesc);
            builder.WriteColour(colour, LoadOp::Clear, ClearValue::FromColour(clearColour));
            builder.WriteDepth(depth, LoadOp::Clear, 1.0f);

            return [this, &params, sceneGlobals, hasCamera](RenderDevice& device, const RenderGraphResources& /*resources*/)
            {
                if (!hasCamera || !m_context)
                {
                    return;
                }

                SubmissionDrawState state{
                    .Device = device,
                    .Pipelines = m_context->GetPipelines(),
                    .Shaders = m_context->GetShaders(),
                    .Programs = m_context->GetPrograms(),
                    .Params = params,
                };

                for (const auto& layer : params.Frame.Layers)
                {
                    if (!layer.Enabled || layer.Kind != FrameLayerKind::Scene)
                    {
                        continue;
                    }

                    const auto resolved = Rendering::ResolveViewForLayer(params, layer.ViewIndex);
                    if (!resolved.IsValid)
                    {
                        continue;
                    }

                    for (const auto& submission : layer.Meshes)
                    {
                        DrawSubmission(state, submission, resolved.View, resolved.ProjectionMatrix, sceneGlobals);
                    }
                }
            };
        });
    }

#ifdef WAYFINDER_COMPILER_MSVC
#pragma warning(pop)
#endif

} // namespace Wayfinder
