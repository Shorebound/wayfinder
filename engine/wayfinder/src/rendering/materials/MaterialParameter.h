#pragma once

#include "core/Types.h"
#include "wayfinder_exports.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Wayfinder
{
    // ── Material Parameter Types ─────────────────────────────
    // A material carries a generic parameter bag populated from JSON
    // and consumed by the shader program to fill UBO bytes.

    enum class MaterialParamType : uint8_t
    {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Colour, // LinearColour (float4, same GPU layout as Vec4)
        Int,
    };

    using MaterialParamValue = std::variant<float, Float2, Float3, Float4, LinearColour, int32_t>;

    // ── Material Parameter Declaration ───────────────────────
    // Declared by a ShaderProgram to describe what parameters it expects.
    // Provides name, type, byte offset into the UBO, and a default value.

    struct MaterialParamDecl
    {
        std::string Name;
        MaterialParamType Type = MaterialParamType::Float;
        uint32_t Offset = 0; // Byte offset into the fragment material UBO
        MaterialParamValue Default;
    };

    // ── Material Parameter Block ─────────────────────────────
    // Runtime storage for material parameters, keyed by name.
    // Authored materials populate this from JSON parameter objects.
    // The renderer serialises it into UBO bytes using the shader program's declarations.

    struct WAYFINDER_API MaterialParameterBlock
    {
        std::unordered_map<std::string, MaterialParamValue> Values;

        void SetFloat(std::string_view name, float v)
        {
            Values[std::string(name)] = v;
        }
        void SetVec2(std::string_view name, const Float2& v)
        {
            Values[std::string(name)] = v;
        }
        void SetVec3(std::string_view name, const Float3& v)
        {
            Values[std::string(name)] = v;
        }
        void SetVec4(std::string_view name, const Float4& v)
        {
            Values[std::string(name)] = v;
        }
        void SetColour(std::string_view name, const LinearColour& v)
        {
            Values[std::string(name)] = v;
        }
        void SetInt(std::string_view name, int32_t v)
        {
            Values[std::string(name)] = v;
        }

        bool Has(std::string_view name) const
        {
            return Values.contains(std::string(name));
        }

        // Write all parameters into a byte buffer using the given declarations.
        // Unknown parameters are skipped; missing parameters use the declaration's default.
        void SerialiseToUBO(const std::vector<MaterialParamDecl>& decls, void* outBuffer, uint32_t bufferSize) const;
    };

} // namespace Wayfinder
