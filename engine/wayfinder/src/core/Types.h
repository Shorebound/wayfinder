#pragma once

/**
 * @brief Fundamental engine types — math aliases and colour primitives.
 *
 * This header is dependency-free within the engine (no rendering, scene, or
 * platform includes) so that any module can use these types without pulling
 * in heavier subsystems.
 */

#include <cstdint>

#include <glm/glm.hpp>

namespace Wayfinder
{

    // ── Engine Math Aliases ──────────────────────────────────

    using Float3 = glm::vec3;
    using Float4 = glm::vec4;
    using Matrix4 = glm::mat4;

    // ── Colour ────────────────────────────────────────────────

    /** @brief 8-bit sRGB colour (authored / on-disk representation). */
    struct Colour
    {
        uint8_t r, g, b, a;

        static Colour White() { return {.r = 255, .g = 255, .b = 255, .a = 255}; }
        static Colour Black() { return {.r = 0, .g = 0, .b = 0, .a = 255}; }
        static Colour Red() { return {.r = 255, .g = 0, .b = 0, .a = 255}; }
        static Colour Green() { return {.r = 0, .g = 255, .b = 0, .a = 255}; }
        static Colour Blue() { return {.r = 0, .g = 0, .b = 255, .a = 255}; }
        static Colour Yellow() { return {.r = 255, .g = 255, .b = 0, .a = 255}; }
        static Colour Gray() { return {.r = 128, .g = 128, .b = 128, .a = 255}; }
        static Colour DarkGray() { return {.r = 80, .g = 80, .b = 80, .a = 255}; }
    };

    // ── LinearColour ─────────────────────────────────────────

    /** @brief Float colour in linear space for GPU-side work.
     *
     * Conversions from authored sRGB colour happen once at load/extract time.
     */
    struct LinearColour
    {
        float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

        static LinearColour White() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
        static LinearColour Black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

        static LinearColour FromColour(const Colour& c)
        {
            return {
                .r = static_cast<float>(c.r) / 255.0f,
                .g = static_cast<float>(c.g) / 255.0f,
                .b = static_cast<float>(c.b) / 255.0f,
                .a = static_cast<float>(c.a) / 255.0f,
            };
        }

        glm::vec4 ToVec4() const { return {r, g, b, a}; }
        Float3 ToFloat3() const { return {r, g, b}; }
    };

} // namespace Wayfinder
