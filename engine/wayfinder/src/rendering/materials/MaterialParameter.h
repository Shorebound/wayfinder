#pragma once

#include "core/TransparentStringHash.h"
#include "core/Types.h"
#include "wayfinder_exports.h"

#include <cstdint>
#include <functional>
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
        /// Heterogeneous lookup avoids allocating a temporary `std::string` on `find` / `contains` when the key exists.
        std::unordered_map<std::string, MaterialParamValue, TransparentStringHash, std::equal_to<>> Values;

        /// Flat slot-indexed storage, built from a shader program's declarations.
        /// When non-empty, SerialiseToUBO reads from here (O(n), no hash lookups).
        ///
        /// @note Slots are only valid between BuildSlots() and the next mutation of
        /// Values (SetFloat, SetVec3, etc.). Callers that modify Values after
        /// BuildSlots must call BuildSlots again, or clear Slots to fall back to
        /// the named-lookup path.
        std::vector<MaterialParamValue> Slots;

    private:
        void SetParameter(std::string_view name, MaterialParamValue value)
        {
            if (const auto it = Values.find(name); it != Values.end())
            {
                it->second = std::move(value);
            }
            else
            {
                Values.emplace(std::string(name), std::move(value));
            }
            Slots.clear(); // Invalidate flat cache — caller must BuildSlots again.
        }

    public:
        void SetFloat(std::string_view name, float v)
        {
            SetParameter(name, v);
        }
        void SetVec2(std::string_view name, const Float2& v)
        {
            SetParameter(name, v);
        }
        void SetVec3(std::string_view name, const Float3& v)
        {
            SetParameter(name, v);
        }
        void SetVec4(std::string_view name, const Float4& v)
        {
            SetParameter(name, v);
        }
        void SetColour(std::string_view name, const LinearColour& v)
        {
            SetParameter(name, v);
        }
        void SetInt(std::string_view name, int32_t v)
        {
            SetParameter(name, v);
        }

        bool Has(std::string_view name) const
        {
            return Values.contains(name);
        }

        /**
         * @brief Populate the flat Slots vector from the named Values map.
         *
         * Each slot corresponds to a declaration in order. Missing values use
         * the declaration's default.
         * @param decls Shader parameter declarations defining the expected layout.
         */
        void BuildSlots(const std::vector<MaterialParamDecl>& decls);

        /**
         * @brief Apply named overrides from another block into the flat Slots.
         *
         * Uses the declarations to resolve name to slot index. Slots must be
         * populated via BuildSlots before calling this.
         * @param overrides Source block of named overrides to apply.
         * @param decls Shader parameter declarations used to resolve names.
         */
        void ApplyOverrides(const MaterialParameterBlock& overrides, const std::vector<MaterialParamDecl>& decls);

        /**
         * @brief Write all parameters into a byte buffer using the given declarations.
         *
         * Uses the fast Slots path when available (exact size match with decls),
         * otherwise falls back to named Values lookup. Unknown parameters are
         * skipped; missing parameters use the declaration's default.
         * @param decls Shader parameter declarations defining offsets and types.
         * @param outBuffer Destination byte buffer.
         * @param bufferSize Size of the destination buffer in bytes.
         */
        void SerialiseToUBO(const std::vector<MaterialParamDecl>& decls, void* outBuffer, uint32_t bufferSize) const;
    };

} // namespace Wayfinder
