#pragma once

namespace Wayfinder
{
    // Forward declarations
    class Input;
    class Time;
    class IGraphicsContext;
    class IRenderAPI;

    // Service locator pattern for accessing engine services
    class WAYFINDER_API ServiceLocator
    {
    public:
        static void Initialize();
        static void Shutdown();

        static Input& GetInput();
        static Time& GetTime();
        static IGraphicsContext& GetGraphicsContext();
        static IRenderAPI& GetRenderAPI();

    private:
        static std::unique_ptr<Input> s_Input;
        static std::unique_ptr<Time> s_Time;
        static std::unique_ptr<IGraphicsContext> s_GraphicsContext;
        static std::unique_ptr<IRenderAPI> s_RenderAPI;
    };
}
