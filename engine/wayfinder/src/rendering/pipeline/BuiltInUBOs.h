#pragma once

#include "core/Types.h"
#include "rendering/materials/RenderingEffects.h"

#include <cstdint>
#include <type_traits>

namespace Wayfinder
{
    /// Vertex UBO for lit scene shaders: MVP + model (matches basic_lit / textured_lit vertex stage).
    struct TransformUBO
    {
        Matrix4 Mvp{};
        Matrix4 Model{};
    };
    static_assert(sizeof(TransformUBO) == 128, "TransformUBO must match shader layout (2 x mat4)");

    /// Vertex UBO for unlit scene shaders (single MVP matrix).
    struct UnlitTransformUBO
    {
        Matrix4 Mvp{};
    };
    static_assert(sizeof(UnlitTransformUBO) == 64, "UnlitTransformUBO must match shader layout (1 x mat4)");

    /// Fragment UBO for debug unlit draws (base colour).
    struct DebugMaterialUBO
    {
        Float4 BaseColour{};
    };
    static_assert(sizeof(DebugMaterialUBO) == 16, "DebugMaterialUBO must match shader layout (vec4)");

    /// Per-frame scene globals pushed to fragment UBO slot 1 for shaders that need it (std140).
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

    /// Fragment UBO for fullscreen composition (std140, matches `composition.frag`).
    struct alignas(16) CompositionUBO
    {
        Float4 ExposureContrastSaturationPad{}; ///< x = exposure (stops), y = contrast, z = saturation
        Float4 Lift{};
        Float4 Gamma{};
        Float4 Gain{};
        Float4 VignetteAberrationPad{}; ///< x = vignette strength, y = chromatic aberration intensity
    };
    static_assert(sizeof(CompositionUBO) == 80, "CompositionUBO must match composition.frag cbuffer");

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
