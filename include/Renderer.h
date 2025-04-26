#pragma once

#include "raylib.h"
#include <memory>

namespace Wayfinder {

// Forward declarations
class Scene;

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Renderer lifecycle methods
    bool Initialize(int screenWidth, int screenHeight);
    void Shutdown();

    // Main render method - renders the entire scene
    void Render(const Scene& scene);

    // Camera management
    void SetCameraPosition(float x, float y, float z);
    void SetCameraTarget(float x, float y, float z);

private:
    // Internal rendering methods
    void BeginRenderFrame();
    void EndRenderFrame();
    void RenderEntities(const Scene& scene);

    // Camera settings
    Camera3D m_camera;
    
    // Screen dimensions
    int m_screenWidth;
    int m_screenHeight;
    
    // Renderer state
    bool m_isInitialized;
};

} // namespace Wayfinder
