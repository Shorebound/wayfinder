#include "Entity.h"
#include "WayfinderPCH.h"
#include "scene/Components.h"
#include "scene/Scene.h"

namespace Wayfinder
{
    Entity::Entity(flecs::entity handle, const Scene* scene) : m_entityHandle(handle), m_scene(scene) {}

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
        const std::string previousName = hadPreviousName ? m_entityHandle.get<NameComponent>().Value : std::string{};

        const std::string finalName = (m_scene != nullptr) ? m_scene->GenerateUniqueName(name, m_entityHandle.id()) : name;

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

    Result<void> Entity::SetSceneObjectId(const SceneObjectId& id)
    {
        if (id.IsNil() && m_scene != nullptr && m_entityHandle.has<SceneOwnership>(m_scene->GetSceneTag()))
        {
            return MakeError("Cannot set nil SceneObjectId on a scene-owned entity");
        }

        const SceneObjectId previousId = GetSceneObjectId();
        m_entityHandle.set<SceneObjectIdComponent>({id});

        if (m_scene != nullptr)
        {
            m_scene->UpdateEntityId(m_entityHandle, previousId, id);
        }

        return {};
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
