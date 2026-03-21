#pragma once

#include <glm/glm.hpp>

namespace Wayfinder
{

    // ── Engine Math Aliases ──────────────────────────────────
    //
    // Thin aliases over GLM types.  These live in core/ so that any
    // subsystem (physics, scene, maths, rendering, …) can reference
    // them without pulling in rendering-specific headers.

    using Float3 = glm::vec3;
    using Float4 = glm::vec4;
    using Matrix4 = glm::mat4;

} // namespace Wayfinder
