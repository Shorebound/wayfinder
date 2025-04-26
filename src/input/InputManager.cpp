#include "InputManager.h"

namespace Wayfinder {

InputManager::InputManager()
    : m_isInitialized(false)
{
}

InputManager::~InputManager()
{
    if (m_isInitialized)
    {
        Shutdown();
    }
}

bool InputManager::Initialize()
{
    // Initialize input manager
    TraceLog(LOG_INFO, "Initializing input manager");

    m_isInitialized = true;
    return true;
}

void InputManager::Update()
{
    // Update input state
    // This is handled by raylib internally
}

void InputManager::Shutdown()
{
    // Clean up input manager resources
    TraceLog(LOG_INFO, "Shutting down input manager");

    m_isInitialized = false;
}

bool InputManager::IsKeyPressed(int key) const
{
    return ::IsKeyPressed(key);
}

bool InputManager::IsKeyDown(int key) const
{
    return ::IsKeyDown(key);
}

bool InputManager::IsKeyReleased(int key) const
{
    return ::IsKeyReleased(key);
}

bool InputManager::IsKeyUp(int key) const
{
    return ::IsKeyUp(key);
}

bool InputManager::IsMouseButtonPressed(int button) const
{
    return ::IsMouseButtonPressed(button);
}

bool InputManager::IsMouseButtonDown(int button) const
{
    return ::IsMouseButtonDown(button);
}

bool InputManager::IsMouseButtonReleased(int button) const
{
    return ::IsMouseButtonReleased(button);
}

bool InputManager::IsMouseButtonUp(int button) const
{
    return ::IsMouseButtonUp(button);
}

Vector2 InputManager::GetMousePosition() const
{
    return ::GetMousePosition();
}

Vector2 InputManager::GetMouseDelta() const
{
    return ::GetMouseDelta();
}

float InputManager::GetMouseWheelMove() const
{
    return ::GetMouseWheelMove();
}

} // namespace Wayfinder
