#pragma once

#include "Component.h"
#include "raylib.h"
#include "raymath.h"

namespace Wayfinder
{

    class Transform : public Component
    {
    public:
        Transform();
        virtual ~Transform();

        virtual void Initialize() override;
        virtual void Update(float deltaTime) override;
        virtual void Shutdown() override;

        Vector3 GetPosition() const { return m_position; }
        void SetPosition(const Vector3& position) { m_position = position; }
        void SetPosition(float x, float y, float z) { m_position = {x, y, z}; }

        Vector3 GetRotation() const { return m_rotation; }
        void SetRotation(const Vector3& rotation) { m_rotation = rotation; }
        void SetRotation(float x, float y, float z) { m_rotation = {x, y, z}; }

        Vector3 GetScale() const { return m_scale; }
        void SetScale(const Vector3& scale) { m_scale = scale; }
        void SetScale(float x, float y, float z) { m_scale = {x, y, z}; }
        void SetScale(float uniformScale) { m_scale = {uniformScale, uniformScale, uniformScale}; }

        // Transform matrix
        Matrix GetTransformMatrix() const;

    private:
        Vector3 m_position;
        Vector3 m_rotation;
        Vector3 m_scale;
    };

} // namespace Wayfinder
