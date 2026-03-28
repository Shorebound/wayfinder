#pragma once

#include "rendering/materials/RenderingEffects.h"
#include "rendering/pipeline/BuiltInUBOs.h"

namespace Wayfinder
{
    /**
     * @brief Pack CPU post-process params into the std140 layout expected by `composition.frag`.
     * Uses Override::Value fields (identity defaults apply when overrides were inactive through the blend).
     */
    [[nodiscard]] inline CompositionUBO MakeCompositionUBO(const ColourGradingParams& grading, const VignetteParams& vignette, const ChromaticAberrationParams& ca)
    {
        CompositionUBO u{};
        u.ExposureContrastSaturationPad = Float4(grading.ExposureStops.Value, grading.Contrast.Value, grading.Saturation.Value, 0.0f);
        u.Lift = Float4(grading.Lift.Value.x, grading.Lift.Value.y, grading.Lift.Value.z, 0.0f);
        u.Gamma = Float4(grading.Gamma.Value.x, grading.Gamma.Value.y, grading.Gamma.Value.z, 0.0f);
        u.Gain = Float4(grading.Gain.Value.x, grading.Gain.Value.y, grading.Gain.Value.z, 0.0f);
        u.VignetteAberrationPad = Float4(vignette.Strength.Value, ca.Intensity.Value, 0.0f, 0.0f);
        return u;
    }
} // namespace Wayfinder
