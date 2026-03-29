#pragma once

#include "core/Types.h"
#include "rendering/pipeline/BuiltInUBOs.h"
#include "rendering/pipeline/FrameRenderParams.h"

#include <cstdint>
#include <vector>

namespace Wayfinder
{
    class Mesh;
    class PipelineCache;
    class RenderDevice;
    class ShaderManager;
    class ShaderProgramRegistry;
    struct RenderMeshSubmission;
    struct ShaderProgram;

    struct SubmissionDrawState
    {
        RenderDevice& Device;
        PipelineCache& Pipelines;
        ShaderManager& Shaders;
        ShaderProgramRegistry& Programs;
        const FrameRenderParams& Params;
        const ShaderProgram* LastBoundProgram = nullptr;
        std::vector<uint8_t> MaterialUBOScratch;
    };

    /// Draws a single submission (solid + optional wireframe).
    /// Resolves shader, binds mesh, pushes UBOs, binds textures, issues draw.
    void DrawSubmission(SubmissionDrawState& state, const RenderMeshSubmission& submission, const Matrix4& viewMatrix, const Matrix4& projectionMatrix, const SceneGlobalsUBO& sceneGlobals);

} // namespace Wayfinder
