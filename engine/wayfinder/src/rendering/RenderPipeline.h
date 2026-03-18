#pragma once

#include "RenderFrame.h"

namespace Wayfinder
{
    class RenderDevice;
    class RenderResourceCache;

    class WAYFINDER_API RenderPipeline
    {
    public:
        void Execute(const RenderFrame& frame, RenderDevice& device, RenderResourceCache& resources) const;
    };
}