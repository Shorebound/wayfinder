#pragma once

#include "rendering/materials/PostProcessVolume.h"
#include "rendering/pipeline/ShaderUniforms.h"

namespace Wayfinder
{
    /** @brief Pack CPU colour grading into the std140 layout expected by `composition.frag`. */
    [[nodiscard]] inline CompositionUBO MakeCompositionUBO(const ColourGradingParams& p)
    {
        CompositionUBO u{};
        u.ExposureContrastSaturationPad = Float4(p.ExposureStops, p.Contrast, p.Saturation, 0.0f);
        u.Lift = Float4(p.Lift.x, p.Lift.y, p.Lift.z, 0.0f);
        u.Gamma = Float4(p.Gamma.x, p.Gamma.y, p.Gamma.z, 0.0f);
        u.Gain = Float4(p.Gain.x, p.Gain.y, p.Gain.z, 0.0f);
        u.VignetteAberrationPad = Float4(p.VignetteStrength, p.ChromaticAberrationIntensity, 0.0f, 0.0f);
        return u;
    }
} // namespace Wayfinder
