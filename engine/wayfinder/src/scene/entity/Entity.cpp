#include "WayfinderPCH.h"
#include "Entity.h"
#include "scene/Components.h"
#include "scene/Scene.h"

namespace Wayfinder
{
    Entity::Entity(flecs::entity handle, const Scene* scene)
        : m_entityHandle(handle), m_scene(scene)
    {
    }

    std::string Entity::GetName() const
    {
        if (m_entityHandle.has<NameComponent>())
        {
            return m_entityHandle.get<NameComponent>().Value;
        }

        return "Unnamed Entity";
    }

    void Entity::SetName(const std::string& name)
    {
        const bool hadPreviousName = m_entityHandle.has<NameComponent>();
        const std::string previousName = hadPreviousName
            ? m_entityHandle.get<NameComponent>().Value
            : std::string{};

        /// Deduplicate against the scene's name index, skipping the
        /// current entity so that re-setting the same name is a no-op.
        std::string finalName = name;
        if (m_scene != nullptr)
        {
            auto it = m_scene->m_entitiesByName.find(finalName);
            if (it != m_scene->m_entitiesByName.end() && it->second != m_entityHandle.id())
            {
                uint32_t suffix = 1;
                do
                {
                    finalName = name + std::to_string(suffix);
                    it = m_scene->m_entitiesByName.find(finalName);
                    ++suffix;
                }
                while (it != m_scene->m_entitiesByName.end() && it->second != m_entityHandle.id());
            }
        }

        if (m_entityHandle.has<NameComponent>())
        {
            m_entityHandle.get_mut<NameComponent>().Value = finalName;
        }
        else
        {
            m_entityHandle.set<NameComponent>(NameComponent{finalName});
        }

        if (m_scene != nullptr)
        {
            if (hadPreviousName)
            {
                m_scene->UpdateEntityName(m_entityHandle, previousName, finalName);
            }
            else
            {
                m_scene->RegisterEntityName(m_entityHandle, finalName);
            }
        }
    }

    bool Entity::HasSceneObjectId() const
    {
        return m_entityHandle.has<SceneObjectIdComponent>();
    }

    SceneObjectId Entity::GetSceneObjectId() const
    {
        if (m_entityHandle.has<SceneObjectIdComponent>())
        {
            return m_entityHandle.get<SceneObjectIdComponent>().Value;
        }

        return {};
    }

    void Entity::SetSceneObjectId(const SceneObjectId& id)
    {
        const SceneObjectId previousId = GetSceneObjectId();
        m_entityHandle.set<SceneObjectIdComponent>({id});

        if (m_scene != nullptr)
        {
            m_scene->UpdateEntityId(m_entityHandle, previousId, id);
        }
    }

    bool Entity::HasPrefabAssetId() const
    {
        return m_entityHandle.has<PrefabInstanceComponent>() && !m_entityHandle.get<PrefabInstanceComponent>().SourceAssetId.IsNil();
    }

    AssetId Entity::GetPrefabAssetId() const
    {
        if (m_entityHandle.has<PrefabInstanceComponent>())
        {
            return m_entityHandle.get<PrefabInstanceComponent>().SourceAssetId;
        }

        return {};
    }

    void Entity::SetPrefabAssetId(const AssetId& id)
    {
        m_entityHandle.set<PrefabInstanceComponent>({id});
    }
}
