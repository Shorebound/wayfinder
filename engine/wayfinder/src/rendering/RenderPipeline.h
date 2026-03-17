#pragma once

#include "RenderFrame.h"

namespace Wayfinder
{
    class IRenderAPI;
    class RenderResourceCache;

    class WAYFINDER_API RenderPipeline
    {
    public:
        void Execute(const RenderFrame& frame, IRenderAPI& renderAPI, RenderResourceCache& resources) const;

    private:
        void ExecutePass(const RenderPass& pass, const RenderFrame& frame, IRenderAPI& renderAPI, RenderResourceCache& resources) const;
        void ExecuteScenePass(const RenderPass& pass, const RenderView& view, IRenderAPI& renderAPI, RenderResourceCache& resources) const;
        void ExecuteDebugPass(const RenderPass& pass, const RenderView& view, IRenderAPI& renderAPI) const;
        void DrawMeshSubmission(const RenderMeshSubmission& mesh, IRenderAPI& renderAPI, RenderResourceCache& resources) const;
        void DrawDebugBox(const RenderDebugBox& debugBox, IRenderAPI& renderAPI) const;
        void ApplyMaterialBinding(const RenderMaterialBinding& binding, const Matrix4& transform, const Float3& dimensions, IRenderAPI& renderAPI) const;
    };
}