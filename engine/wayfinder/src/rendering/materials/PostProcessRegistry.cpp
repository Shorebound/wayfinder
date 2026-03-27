#include "PostProcessRegistry.h"

#include <algorithm>

namespace Wayfinder
{
    namespace
    {
        PostProcessRegistry* g_activeInstance = nullptr;
    }

    void PostProcessRegistry::SetActiveInstance(PostProcessRegistry* registry)
    {
        g_activeInstance = registry;
    }

    PostProcessRegistry* PostProcessRegistry::GetActiveInstance()
    {
        return g_activeInstance;
    }

    void PostProcessRegistry::Seal()
    {
        m_sealed = true;
    }

    const PostProcessEffectDesc* PostProcessRegistry::Find(const PostProcessEffectId id) const
    {
        const auto index = static_cast<std::size_t>(id);
        if (index >= m_descs.size())
        {
            return nullptr;
        }
        return &m_descs[index];
    }

    std::optional<PostProcessEffectId> PostProcessRegistry::FindIdByName(const std::string_view name) const
    {
        for (std::size_t i = 0; i < m_descs.size(); ++i)
        {
            if (m_descs[i].Name == name)
            {
                return static_cast<PostProcessEffectId>(i);
            }
        }
        return std::nullopt;
    }

} // namespace Wayfinder
