#pragma once

#include "core/Types.h"
#include "rendering/pipeline/RenderPipelineFrameParams.h"

#include <cstdint>
#include <type_traits>
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

    /// Per-frame scene globals pushed to fragment UBO slot 1 for shaders that need it.
    struct alignas(16) SceneGlobalsUBO
    {
        Float3 LightDirection{0.0f, -0.7f, -0.5f};
        float LightIntensity = 1.0f;
        Float3 LightColour{1.0f, 1.0f, 1.0f};
        float Ambient = 0.15f;
    };
    static_assert(std::is_standard_layout_v<SceneGlobalsUBO>, "SceneGlobalsUBO must be standard layout for GPU upload");
    static_assert(std::is_trivially_copyable_v<SceneGlobalsUBO>, "SceneGlobalsUBO must be trivially copyable for GPU upload");
    static_assert(sizeof(SceneGlobalsUBO) == 32, "SceneGlobalsUBO must be 32 bytes (2 x vec4) for std140 layout");

    struct SubmissionDrawState
    {
        RenderDevice& Device;
        PipelineCache& Pipelines;
        ShaderManager& Shaders;
        ShaderProgramRegistry& Programs;
        const RenderPipelineFrameParams& Params;
        const ShaderProgram* LastBoundProgram = nullptr;
        std::vector<uint8_t> MaterialUBOScratch;
    };

    /// Draws a single submission (solid + optional wireframe).
    /// Resolves shader, binds mesh, pushes UBOs, binds textures, issues draw.
    void DrawSubmission(SubmissionDrawState& state, const RenderMeshSubmission& submission, const Matrix4& viewMatrix, const Matrix4& projectionMatrix, const SceneGlobalsUBO& sceneGlobals);

} // namespace Wayfinder
