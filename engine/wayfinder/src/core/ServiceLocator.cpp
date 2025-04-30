#include "ServiceLocator.h"
#include "../platform/Window.h"
#include "../platform/Input.h"
#include "../platform/Time.h"
#include "../rendering/GraphicsContext.h"
#include "../rendering/RenderAPI.h"

namespace Wayfinder
{
    // Initialize static members
    std::unique_ptr<Input> ServiceLocator::s_input = nullptr;
    std::unique_ptr<Time> ServiceLocator::s_time = nullptr;
    std::unique_ptr<IGraphicsContext> ServiceLocator::s_graphicsContext = nullptr;
    std::unique_ptr<IRenderAPI> ServiceLocator::s_renderAPI = nullptr;

    void ServiceLocator::Initialize()
    {
        s_input = Input::Create();
        s_time = Time::Create();
        s_graphicsContext = IGraphicsContext::Create();
        s_renderAPI = IRenderAPI::Create();

        s_graphicsContext->Initialize();
        s_renderAPI->Initialize();
    }

    void ServiceLocator::Shutdown()
    {
        if (s_renderAPI)
        {
            s_renderAPI->Shutdown();
            s_renderAPI = nullptr;
        }

        if (s_graphicsContext)
        {
            s_graphicsContext->Shutdown();
            s_graphicsContext = nullptr;
        }

        s_input = nullptr;
        s_time = nullptr;
    }

    Input& ServiceLocator::GetInput()
    {
        return *s_input;
    }

    Time& ServiceLocator::GetTime()
    {
        return *s_time;
    }

    IGraphicsContext& ServiceLocator::GetGraphicsContext()
    {
        return *s_graphicsContext;
    }

    IRenderAPI& ServiceLocator::GetRenderAPI()
    {
        return *s_renderAPI;
    }
}
