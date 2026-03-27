#include "BlendableEffectRegistry.h"

namespace Wayfinder
{
    namespace
    {
        BlendableEffectRegistry* g_activeInstance = nullptr;
    }

    void BlendableEffectRegistry::SetActiveInstance(BlendableEffectRegistry* registry)
    {
        g_activeInstance = registry;
    }

    BlendableEffectRegistry* BlendableEffectRegistry::GetActiveInstance()
    {
        return g_activeInstance;
    }

    void BlendableEffectRegistry::Seal()
    {
        m_sealed = true;
    }

    const BlendableEffectDesc* BlendableEffectRegistry::Find(const BlendableEffectId id) const
    {
        if (id >= m_descs.size())
        {
            return nullptr;
        }
        return &m_descs[id];
    }

    std::optional<BlendableEffectId> BlendableEffectRegistry::FindIdByName(const std::string_view name) const
    {
        for (std::size_t i = 0; i < m_descs.size(); ++i)
        {
            if (m_descs[i].Name == name)
            {
                return static_cast<BlendableEffectId>(i);
            }
        }
        return std::nullopt;
    }

} // namespace Wayfinder
