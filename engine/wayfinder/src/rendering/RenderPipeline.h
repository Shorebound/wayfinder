#pragma once

#include "RenderFrame.h"

namespace Wayfinder
{
    class RenderDevice;
    class RenderResourceCache;

    // Validates and sorts the render frame's passes before drawing.
    // Stage 6 will evolve this into a full render graph builder.
    class WAYFINDER_API RenderPipeline
    {
    public:
        // Validates views/passes, sorts scene submissions by sort key.
        // Returns false if the frame is invalid and should be skipped.
        bool Prepare(RenderFrame& frame) const;
    };
}