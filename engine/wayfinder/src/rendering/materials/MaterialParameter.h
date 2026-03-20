#pragma once

#include "rendering/RenderTypes.h"
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
    // A material carries a generic parameter bag populated from TOML
    // and consumed by the shader program to fill UBO bytes.

    enum class MaterialParamType : uint8_t
    {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Color, // LinearColor (float4, same GPU layout as Vec4)
        Int,
    };

    using MaterialParamValue = std::variant<
        float,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        LinearColor,
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
    // Authored materials populate this from TOML [parameters].
    // The renderer serializes it into UBO bytes using the shader program's declarations.

    struct WAYFINDER_API MaterialParameterBlock
    {
        std::unordered_map<std::string, MaterialParamValue> Values;

        void SetFloat(const std::string& name, float v) { Values[name] = v; }
        void SetVec2(const std::string& name, const glm::vec2& v) { Values[name] = v; }
        void SetVec3(const std::string& name, const glm::vec3& v) { Values[name] = v; }
        void SetVec4(const std::string& name, const glm::vec4& v) { Values[name] = v; }
        void SetColor(const std::string& name, const LinearColor& v) { Values[name] = v; }
        void SetInt(const std::string& name, int32_t v) { Values[name] = v; }

        bool Has(const std::string& name) const { return Values.contains(name); }

        // Write all parameters into a byte buffer using the given declarations.
        // Unknown parameters are skipped; missing parameters use the declaration's default.
        void SerializeToUBO(const std::vector<MaterialParamDecl>& decls, void* outBuffer, uint32_t bufferSize) const;
    };

} // namespace Wayfinder
