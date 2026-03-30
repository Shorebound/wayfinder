#pragma once

#include "core/Types.h"

#include <cstdint>

namespace Wayfinder
{
    struct RenderFrame;
}

namespace Wayfinder::Rendering
{
    /**
     * @brief Validates views/layers, pre-computes view matrices and frustums,
     *        frustum-culls submissions, then sorts by sort key.
     * @param frame The render frame to prepare (modified in place).
     * @param swapchainWidth Current swapchain width in pixels.
     * @param swapchainHeight Current swapchain height in pixels.
     * @return False if the frame is invalid and should be skipped.
     */
    bool PrepareFrame(RenderFrame& frame, uint32_t swapchainWidth, uint32_t swapchainHeight);

} // namespace Wayfinder::Rendering
