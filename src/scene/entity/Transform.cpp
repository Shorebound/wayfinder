#include "Transform.h"
#include "Entity.h"

namespace Wayfinder
{

    Transform::Transform()
        : m_position({0.0f, 0.0f, 0.0f}), m_rotation({0.0f, 0.0f, 0.0f}), m_scale({1.0f, 1.0f, 1.0f})
    {
    }

    Transform::~Transform()
    {
    }

    void Transform::Initialize()
    {
        Component::Initialize();
    }

    void Transform::Update(float deltaTime)
    {
        Component::Update(deltaTime);

        // Transform-specific update logic can go here
    }

    void Transform::Shutdown()
    {
        Component::Shutdown();
    }

    Matrix Transform::GetTransformMatrix() const
    {
        Matrix translation = MatrixTranslate(m_position.x, m_position.y, m_position.z);

        Matrix rotationX = MatrixRotateX(DEG2RAD * m_rotation.x);
        Matrix rotationY = MatrixRotateY(DEG2RAD * m_rotation.y);
        Matrix rotationZ = MatrixRotateZ(DEG2RAD * m_rotation.z);

        Matrix rotation = MatrixMultiply(MatrixMultiply(rotationX, rotationY), rotationZ);

        Matrix scale = MatrixScale(m_scale.x, m_scale.y, m_scale.z);

        Matrix transform = MatrixMultiply(MatrixMultiply(scale, rotation), translation);

        return transform;
    }

} // namespace Wayfinder
