#pragma once
#include "../GraphicsContext.h"

namespace Wayfinder
{
    class RaylibGraphicsContext : public IGraphicsContext
    {
    public:
        RaylibGraphicsContext() = default;
        virtual ~RaylibGraphicsContext() = default;

        void Initialize() override;
        void Shutdown() override;
        
        void BeginFrame() override;
        void EndFrame() override;
        void Clear(float r, float g, float b, float a = 1.0f) override;
    };
}
