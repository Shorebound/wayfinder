#include "RaylibGraphicsContext.h"
#include "../null/NullGraphicsContext.h"
#include "raylib.h"

namespace Wayfinder
{
    std::unique_ptr<IGraphicsContext> IGraphicsContext::Create(RenderBackend backend)
    {
        switch (backend)
        {
        case RenderBackend::Raylib:
            return std::make_unique<RaylibGraphicsContext>();
        case RenderBackend::Null:
            return std::make_unique<NullGraphicsContext>();
        }

        return nullptr;
    }

    void RaylibGraphicsContext::Initialize()
    {
        // Raylib window initialization is handled by RaylibWindow
    }

    void RaylibGraphicsContext::Shutdown()
    {
        // Raylib window shutdown is handled by RaylibWindow
    }

    const RenderBackendCapabilities& RaylibGraphicsContext::GetCapabilities() const
    {
        return m_capabilities;
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
