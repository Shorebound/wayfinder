#include "VolumeEffectRegistry.h"

#include <algorithm>

namespace Wayfinder
{
    namespace
    {
        VolumeEffectRegistry* g_activeInstance = nullptr;
    }

    void VolumeEffectRegistry::SetActiveInstance(VolumeEffectRegistry* registry)
    {
        g_activeInstance = registry;
    }

    VolumeEffectRegistry* VolumeEffectRegistry::GetActiveInstance()
    {
        return g_activeInstance;
    }

    void VolumeEffectRegistry::Seal()
    {
        m_sealed = true;
    }

    const VolumeEffectDesc* VolumeEffectRegistry::Find(const VolumeEffectId id) const
    {
        const auto index = static_cast<std::size_t>(id);
        if (index >= m_descs.size())
        {
            return nullptr;
        }
        return &m_descs[index];
    }

    std::optional<VolumeEffectId> VolumeEffectRegistry::FindIdByName(const std::string_view name) const
    {
        for (std::size_t i = 0; i < m_descs.size(); ++i)
        {
            if (m_descs[i].Name == name)
            {
                return static_cast<VolumeEffectId>(i);
            }
        }
        return std::nullopt;
    }

} // namespace Wayfinder
