#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include <flecs.h>

#include "core/Identifiers.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class Scene;

    class WAYFINDER_API Entity
    {
    private:

        flecs::entity m_entityHandle;
        const Scene* m_scene = nullptr;

    public:
        Entity() = default;
        Entity(flecs::entity handle, const Scene* scene);
        Entity(const Entity& other) = default;

        template <typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            m_entityHandle.set<T>(T{std::forward<Args>(args)...});
            return m_entityHandle.get_mut<T>();
        }

        template <typename T>
        T& GetComponent()
        {
            return m_entityHandle.get_mut<T>();
        }

        template <typename T>
        const T& GetComponent() const
        {
            return m_entityHandle.get<T>();
        }

        template <typename T>
        bool HasComponent() const
        {
            return m_entityHandle.has<T>();
        }

        template <typename T>
        void RemoveComponent()
        {
            m_entityHandle.remove<T>();
        }

        operator bool() const { return m_entityHandle.is_valid(); }
        operator flecs::entity() const { return m_entityHandle; }
        operator uint64_t() const { return m_entityHandle.id(); }

        flecs::entity GetHandle() const { return m_entityHandle; }

        bool operator==(const Entity& other) const { return m_entityHandle == other.m_entityHandle && m_scene == other.m_scene; }
        bool operator!=(const Entity& other) const { return !(*this == other); }

        bool IsValid() const { return m_entityHandle.is_valid() && m_scene != nullptr; }

        std::string GetName() const;
        void SetName(const std::string& name);
        bool HasSceneObjectId() const;
        SceneObjectId GetSceneObjectId() const;
        void SetSceneObjectId(const SceneObjectId& id);
        bool HasPrefabAssetId() const;
        AssetId GetPrefabAssetId() const;
        void SetPrefabAssetId(const AssetId& id);

    };
}
