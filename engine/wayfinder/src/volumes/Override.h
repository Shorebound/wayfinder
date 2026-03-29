#pragma once

#include "core/Types.h"
#include "maths/Maths.h"

#include <utility>

namespace Wayfinder
{
    /**
     * @brief Authoring/runtime field with optional override: blending only touches fields where Active is true.
     */
    template<typename T>
    struct Override
    {
        T Value{};
        bool Active = false;

        static Override Inactive(T defaultValue)
        {
            return Override{std::move(defaultValue), false};
        }

        static Override Set(T value)
        {
            return Override{std::move(value), true};
        }
    };

    /**
     * @brief Blend one override into another: if source is inactive, current is unchanged; otherwise interp is applied.
     */
    template<typename T, typename InterpFn>
    [[nodiscard]] Override<T> LerpOverride(const Override<T>& current, const Override<T>& source, float t, InterpFn&& interp)
    {
        if (!source.Active)
        {
            return current;
        }
        Override<T> out{};
        out.Value = std::forward<InterpFn>(interp)(current.Value, source.Value, t);
        out.Active = true;
        return out;
    }

    [[nodiscard]] inline Override<float> LerpOverride(const Override<float>& current, const Override<float>& source, float t)
    {
        return LerpOverride(current, source, t, [](float a, float b, float w)
        {
            return Maths::Mix(a, b, w);
        });
    }

    [[nodiscard]] inline Override<Float3> LerpOverride(const Override<Float3>& current, const Override<Float3>& source, float t)
    {
        return LerpOverride(current, source, t, [](const Float3& a, const Float3& b, float w)
        {
            return Maths::Mix(a, b, w);
        });
    }

} // namespace Wayfinder
