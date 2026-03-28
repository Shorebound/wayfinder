#pragma once

#include "core/Types.h"

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

} // namespace Wayfinder
