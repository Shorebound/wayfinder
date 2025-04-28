#include "RaylibGraphicsContext.h"
#include "raylib.h"

namespace Wayfinder
{
    std::unique_ptr<IGraphicsContext> IGraphicsContext::Create()
    {
        return std::make_unique<RaylibGraphicsContext>();
    }

    void RaylibGraphicsContext::Initialize()
    {
        // Raylib window initialization is handled by RaylibWindow
    }

    void RaylibGraphicsContext::Shutdown()
    {
        // Raylib window shutdown is handled by RaylibWindow
    }

    void RaylibGraphicsContext::BeginFrame()
    {
        BeginDrawing();
    }

    void RaylibGraphicsContext::EndFrame()
    {
        EndDrawing();
    }

    void RaylibGraphicsContext::Clear(float r, float g, float b, float a)
    {
        ClearBackground(CLITERAL(::Color){ 
            static_cast<unsigned char>(r * 255), 
            static_cast<unsigned char>(g * 255), 
            static_cast<unsigned char>(b * 255), 
            static_cast<unsigned char>(a * 255) 
        });
    }
}
