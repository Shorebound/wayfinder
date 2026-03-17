#pragma once

#include "RenderFrame.h"

namespace Wayfinder
{
    class IRenderAPI;

    class WAYFINDER_API RenderPipeline
    {
    public:
        void Execute(const RenderFrame& frame, const Camera& camera, IRenderAPI& renderAPI) const;

    private:
        void ExecuteScenePass(const RenderFrame& frame, const Camera& camera, IRenderAPI& renderAPI) const;
        void ExecuteDebugPass(const RenderFrame& frame, const Camera& camera, IRenderAPI& renderAPI) const;
        void DrawMeshSubmission(const RenderMeshSubmission& mesh, IRenderAPI& renderAPI) const;
        void DrawDebugBox(const RenderDebugBox& debugBox, IRenderAPI& renderAPI) const;
        void ApplyMaterialBinding(const RenderMaterialBinding& binding, const Matrix4& transform, const Float3& dimensions, IRenderAPI& renderAPI) const;
    };
}