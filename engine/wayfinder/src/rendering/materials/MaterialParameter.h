#pragma once

#include "core/Types.h"
#include "wayfinder_exports.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

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

    using MaterialParamValue = std::variant<
        float,
        Float2,
        Float3,
        Float4,
        LinearColour,
        int32_t>;

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

        void SetFloat(const std::string& name, float v) { Values[name] = v; }
        void SetVec2(const std::string& name, const Float2& v) { Values[name] = v; }
        void SetVec3(const std::string& name, const Float3& v) { Values[name] = v; }
        void SetVec4(const std::string& name, const Float4& v) { Values[name] = v; }
        void SetColour(const std::string& name, const LinearColour& v) { Values[name] = v; }
        void SetInt(const std::string& name, int32_t v) { Values[name] = v; }

        bool Has(const std::string& name) const { return Values.contains(name); }

        // Write all parameters into a byte buffer using the given declarations.
        // Unknown parameters are skipped; missing parameters use the declaration's default.
        void SerialiseToUBO(const std::vector<MaterialParamDecl>& decls, void* outBuffer, uint32_t bufferSize) const;
    };

} // namespace Wayfinder
