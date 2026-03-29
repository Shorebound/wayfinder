#include "MaterialParameter.h"

#include "core/Log.h"

#include <cstring>

namespace Wayfinder
{
    namespace
    {
        void WriteValue(const MaterialParamValue& value, void* dst, uint32_t maxBytes)
        {
            std::visit([dst, maxBytes](auto&& v)
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, float>)
                {
                    if (maxBytes >= sizeof(float))
                    {
                        std::memcpy(dst, &v, sizeof(float));
                    }
                }
                else if constexpr (std::is_same_v<T, Float2>)
                {
                    if (maxBytes >= sizeof(Float2))
                    {
                        std::memcpy(dst, &v, sizeof(Float2));
                    }
                }
                else if constexpr (std::is_same_v<T, Float3>)
                {
                    if (maxBytes >= sizeof(Float3))
                    {
                        std::memcpy(dst, &v, sizeof(Float3));
                    }
                }
                else if constexpr (std::is_same_v<T, Float4>)
                {
                    if (maxBytes >= sizeof(Float4))
                    {
                        std::memcpy(dst, &v, sizeof(Float4));
                    }
                }
                else if constexpr (std::is_same_v<T, LinearColour>)
                {
                    // LinearColour has the same layout as float4
                    if (maxBytes >= sizeof(LinearColour))
                    {
                        std::memcpy(dst, &v, sizeof(LinearColour));
                    }
                }
                else if constexpr (std::is_same_v<T, int32_t>)
                {
                    if (maxBytes >= sizeof(int32_t))
                    {
                        std::memcpy(dst, &v, sizeof(int32_t));
                    }
                }
            }, value);
        }
    }

    void MaterialParameterBlock::BuildSlots(const std::vector<MaterialParamDecl>& decls)
    {
        Slots.resize(decls.size());
        for (size_t i = 0; i < decls.size(); ++i)
        {
            const auto it = Values.find(decls[i].Name);
            Slots[i] = (it != Values.end()) ? it->second : decls[i].Default;
        }
    }

    void MaterialParameterBlock::ApplyOverrides(const MaterialParameterBlock& overrides, const std::vector<MaterialParamDecl>& decls)
    {
        if (Slots.empty())
        {
            WAYFINDER_WARN(LogRenderer, "ApplyOverrides: Slots is empty — call BuildSlots before ApplyOverrides");
            return;
        }

        for (size_t i = 0; i < decls.size(); ++i)
        {
            const auto it = overrides.Values.find(decls[i].Name);
            if (it != overrides.Values.end())
            {
                if (i < Slots.size())
                {
                    Slots[i] = it->second;
                }
            }
        }
    }

    void MaterialParameterBlock::SerialiseToUBO(const std::vector<MaterialParamDecl>& decls, void* outBuffer, uint32_t bufferSize) const
    {
        auto* bytes = static_cast<uint8_t*>(outBuffer);

        if (!Slots.empty() && Slots.size() == decls.size())
        {
            // Fast path: read from pre-built flat slots (no hash lookups)
            for (size_t i = 0; i < decls.size(); ++i)
            {
                const auto& decl = decls[i];
                if (decl.Offset >= bufferSize)
                {
                    continue;
                }
                const uint32_t remaining = bufferSize - decl.Offset;
                const MaterialParamValue& value = Slots[i];
                WriteValue(value, bytes + decl.Offset, remaining);
            }
            return;
        }

        // Fallback: named lookup (authoring path)
        for (const auto& decl : decls)
        {
            if (decl.Offset >= bufferSize)
            {
                continue;
            }

            const uint32_t remaining = bufferSize - decl.Offset;
            auto it = Values.find(decl.Name);
            const MaterialParamValue& value = (it != Values.end()) ? it->second : decl.Default;
            WriteValue(value, bytes + decl.Offset, remaining);
        }
    }

} // namespace Wayfinder
