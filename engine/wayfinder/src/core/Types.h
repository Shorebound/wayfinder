#pragma once

#include <glm/glm.hpp>

namespace Wayfinder
{

    /** @brief 3D float vector for positions, directions, and normals. */
    using Float3 = glm::vec3;

    /** @brief 4D float vector for colours, quaternions, and homogeneous coordinates. */
    using Float4 = glm::vec4;

    /** @brief 4×4 column-major matrix for affine transforms and projections. */
    using Matrix4 = glm::mat4;

} // namespace Wayfinder
