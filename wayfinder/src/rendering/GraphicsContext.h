#pragma once

namespace Wayfinder
{
    // Interface for graphics context
    class WAYFINDER_API IGraphicsContext
    {
    public:
        virtual ~IGraphicsContext() = default;

        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;
        
        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
        virtual void Clear(float r, float g, float b, float a = 1.0f) = 0;
        
        static std::unique_ptr<IGraphicsContext> Create();
    };
}
