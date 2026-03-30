#include "SubmissionDrawing.h"

#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/materials/MaterialParameter.h"
#include "rendering/materials/ShaderManager.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/pipeline/PipelineCache.h"
#include "rendering/resources/RenderResourceCache.h"

#include <optional>

namespace Wayfinder
{
    namespace
    {
        /// Builds a PipelineCreateDesc for the wireframe variant of a shader program.
        /// Returns std::nullopt if the shader handles cannot be resolved.
        std::optional<PipelineCreateDesc> MakeWireframeVariant(const ShaderProgramDesc& desc, ShaderManager& shaders)
        {
            const GPUShaderHandle vs = shaders.GetShader(desc.VertexShaderName, ShaderStage::Vertex, desc.VertexResources);
            const GPUShaderHandle fs = shaders.GetShader(desc.FragmentShaderName, ShaderStage::Fragment, desc.FragmentResources);
            if (!vs || !fs)
            {
                return std::nullopt;
            }

            PipelineCreateDesc pipeDesc{};
            pipeDesc.VertexShader = vs;
            pipeDesc.FragmentShader = fs;
            pipeDesc.VertexLayout = desc.VertexLayout;
            pipeDesc.PrimitiveType = PrimitiveType::TriangleList; // Wireframe always uses triangles
            pipeDesc.CullMode = desc.Cull;
            pipeDesc.FillMode = FillMode::Line;
            pipeDesc.FrontFace = FrontFace::CounterClockwise;
            pipeDesc.DepthTestEnabled = desc.DepthTest;
            pipeDesc.DepthWriteEnabled = desc.DepthWrite;
            return pipeDesc;
        }
    } // namespace

    void DrawSubmission(SubmissionDrawState& state, const RenderMeshSubmission& submission, const Matrix4& passView, const Matrix4& passProj, const SceneGlobalsUBO& sceneGlobals)
    {
        auto& device = state.Device;
        auto& registry = state.Programs;
        auto& pipelineCache = state.Pipelines;
        auto& shaderManager = state.Shaders;
        const auto& params = state.Params;

        const ShaderProgram* program = registry.FindOrDefault(submission.Material.ShaderName);
        if (!program || !program->Pipeline.IsValid())
        {
            return;
        }

        const RenderFillMode fillMode = submission.Material.StateOverrides.FillMode.value_or(RenderFillMode::Solid);

        auto pushUniforms = [&]()
        {
            if (program->Desc.NeedsSceneGlobals)
            {
                TransformUBO transformUBO{};
                transformUBO.Mvp = passProj * passView * submission.LocalToWorld;
                transformUBO.Model = submission.LocalToWorld;
                device.PushVertexUniform(0, &transformUBO, sizeof(TransformUBO));
            }
            else
            {
                UnlitTransformUBO transformUBO{passProj * passView * submission.LocalToWorld};
                device.PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
            }

            state.MaterialUBOScratch.assign(program->Desc.MaterialUBOSize, 0);

            // When flat Slots are pre-built (with overrides already baked in by
            // PrepareFrame), SerialiseToUBO reads linearly — no hash lookups, no
            // per-draw merge copy.  Falls back to named lookup for non-asset materials.
            //
            // Correctness contract: if HasOverrides is true and Slots is non-empty,
            // we assume RenderResourceCache::PrepareMaterialBinding has already baked
            // the overrides into Slots. If a submission is constructed outside the
            // cache path with HasOverrides + pre-built Slots, overrides will be
            // silently ignored.
            if (submission.Material.HasOverrides && submission.Material.Parameters.Slots.empty())
            {
                // Legacy path: overrides weren't baked by PrepareFrame
                MaterialParameterBlock mergedParams = submission.Material.Parameters;
                for (const auto& [name, value] : submission.Material.Overrides.Values)
                {
                    mergedParams.Values[name] = value;
                }
                mergedParams.SerialiseToUBO(program->Desc.MaterialParams, state.MaterialUBOScratch.data(), static_cast<uint32_t>(state.MaterialUBOScratch.size()));
            }
            else
            {
                submission.Material.Parameters.SerialiseToUBO(program->Desc.MaterialParams, state.MaterialUBOScratch.data(), static_cast<uint32_t>(state.MaterialUBOScratch.size()));
            }
            device.PushFragmentUniform(0, state.MaterialUBOScratch.data(), static_cast<uint32_t>(state.MaterialUBOScratch.size()));

            if (program->Desc.NeedsSceneGlobals)
            {
                device.PushFragmentUniform(1, &sceneGlobals, sizeof(SceneGlobalsUBO));
            }
        };

        const RenderMeshResource* meshResource = nullptr;
        if (params.ResourceCache)
        {
            meshResource = &params.ResourceCache->ResolveMesh(submission);
        }

        auto bindMaterialTextures = [&]()
        {
            for (const auto& texBinding : submission.Material.ResolvedTextures)
            {
                if (texBinding.Texture && texBinding.Sampler)
                {
                    device.BindFragmentSampler(texBinding.Slot, texBinding.Texture, texBinding.Sampler);
                }
            }
        };

        const auto resolveMesh = [&]() -> const Mesh*
        {
            if (submission.Mesh.Origin == RenderResourceOrigin::Asset)
            {
                return meshResource ? meshResource->GpuMesh : nullptr;
            }
            const auto& layout = program->Desc.VertexLayout;
            if (VertexLayoutsMatch(layout, VertexLayouts::POSITION_NORMAL_COLOUR))
            {
                return params.BuiltInMeshes[static_cast<size_t>(BuiltInMeshId::PrimitiveColour)];
            }
            if (VertexLayoutsMatch(layout, VertexLayouts::POSITION_NORMAL_UV_TANGENT))
            {
                return params.BuiltInMeshes[static_cast<size_t>(BuiltInMeshId::PrimitiveTextured)];
            }
            return nullptr;
        };

        if (fillMode == RenderFillMode::Solid || fillMode == RenderFillMode::SolidAndWireframe)
        {
            const Mesh* meshPtr = resolveMesh();

            if (!meshPtr || !meshPtr->IsValid())
            {
                return;
            }

            if (program != state.LastBoundProgram)
            {
                device.BindPipeline(program->Pipeline);
                state.LastBoundProgram = program;
            }

            meshPtr->Bind(device);
            pushUniforms();
            bindMaterialTextures();
            meshPtr->Draw(device);
        }

        if (fillMode == RenderFillMode::Wireframe || fillMode == RenderFillMode::SolidAndWireframe)
        {
            const auto wireframeDesc = MakeWireframeVariant(program->Desc, shaderManager);
            if (wireframeDesc)
            {
                const GPUPipelineHandle wireframePipeline = pipelineCache.GetOrCreate(*wireframeDesc);
                if (wireframePipeline.IsValid())
                {
                    const Mesh* wireMeshPtr = resolveMesh();

                    if (wireMeshPtr && wireMeshPtr->IsValid())
                    {
                        device.BindPipeline(wireframePipeline);
                        wireMeshPtr->Bind(device);
                        state.LastBoundProgram = nullptr;
                        bindMaterialTextures();
                        pushUniforms();
                        wireMeshPtr->Draw(device);
                    }
                }
            }
        }
    }

} // namespace Wayfinder
