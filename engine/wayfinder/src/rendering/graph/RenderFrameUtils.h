#pragma once

#include "rendering/RenderTypes.h"

#include <cstddef>

namespace Wayfinder
{
    struct RenderFrame;
    struct FrameRenderParams;
}

namespace Wayfinder::Rendering
{
    /// Resolved camera state for the primary view (`ViewIndex` into `RenderFrame::Views` when that view is prepared).
    struct PreparedPrimaryView
    {
        Matrix4 ViewMatrix = Matrix4(1.0f);
        Matrix4 ProjectionMatrix = Matrix4(1.0f);
        Colour ClearColour = Colour::White();
        /// Index into `RenderFrame::Views` for the view this snapshot was taken from (see `ResolvePreparedPrimaryView`).
        size_t ViewIndex = 0;
        bool Valid = false;
    };

    /** @brief Primary view matrices and clear colour when the resolved primary `RenderView` is prepared; otherwise `Valid` is false. */
    [[nodiscard]] PreparedPrimaryView ResolvePreparedPrimaryView(const RenderFrame& frame);

    /** @brief Resolved view/projection pair for a specific layer view index. */
    struct ResolvedViewForLayer
    {
        Matrix4 View = Matrix4(1.0f);
        Matrix4 ProjectionMatrix = Matrix4(1.0f);
        bool IsValid = false;
    };

    /**
     * @brief Resolves view/projection matrices for a layer's view index.
     *
     * Uses the primary view as the base and overrides with the per-view matrices when
     * the indexed view is prepared. Returns with `IsValid == false` when no valid camera exists.
     */
    [[nodiscard]] ResolvedViewForLayer ResolveViewForLayer(const FrameRenderParams& params, size_t viewIndex);

} // namespace Wayfinder::Rendering
