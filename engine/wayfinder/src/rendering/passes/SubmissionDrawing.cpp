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
            pipeDesc.vertexShader = vs;
            pipeDesc.fragmentShader = fs;
            pipeDesc.vertexLayout = desc.VertexLayout;
            pipeDesc.primitiveType = PrimitiveType::TriangleList;
            pipeDesc.cullMode = desc.Cull;
            pipeDesc.fillMode = FillMode::Line;
            pipeDesc.frontFace = FrontFace::CounterClockwise;
            pipeDesc.depthTestEnabled = desc.DepthTest;
            pipeDesc.depthWriteEnabled = desc.DepthWrite;
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

            MaterialParameterBlock mergedParams = submission.Material.Parameters;
            if (submission.Material.HasOverrides)
            {
                for (const auto& [name, value] : submission.Material.Overrides.Values)
                {
                    mergedParams.Values[name] = value;
                }
            }

            mergedParams.SerialiseToUBO(program->Desc.MaterialParams, state.MaterialUBOScratch.data(), static_cast<uint32_t>(state.MaterialUBOScratch.size()));
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
            const uint32_t stride = program->Desc.VertexLayout.stride;
            if (stride == VertexLayouts::PosNormalColour.stride)
            {
                return params.BuiltInMeshes[static_cast<size_t>(BuiltInMeshId::PrimitiveColour)];
            }
            if (stride == VertexLayouts::PosNormalUVTangent.stride)
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
