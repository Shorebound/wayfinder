#include "ServiceLocator.h"
#include "../platform/Window.h"
#include "../platform/Input.h"
#include "../platform/Time.h"
#include "../rendering/GraphicsContext.h"
#include "../rendering/RenderAPI.h"

namespace Wayfinder
{
    // Initialize static members
    std::unique_ptr<Input> ServiceLocator::s_Input = nullptr;
    std::unique_ptr<Time> ServiceLocator::s_Time = nullptr;
    std::unique_ptr<IGraphicsContext> ServiceLocator::s_GraphicsContext = nullptr;
    std::unique_ptr<IRenderAPI> ServiceLocator::s_RenderAPI = nullptr;

    void ServiceLocator::Initialize()
    {
        s_Input = Input::Create();
        s_Time = Time::Create();
        s_GraphicsContext = IGraphicsContext::Create();
        s_RenderAPI = IRenderAPI::Create();

        s_GraphicsContext->Initialize();
        s_RenderAPI->Initialize();
    }

    void ServiceLocator::Shutdown()
    {
        if (s_RenderAPI)
        {
            s_RenderAPI->Shutdown();
            s_RenderAPI = nullptr;
        }

        if (s_GraphicsContext)
        {
            s_GraphicsContext->Shutdown();
            s_GraphicsContext = nullptr;
        }

        s_Input = nullptr;
        s_Time = nullptr;
    }

    Input& ServiceLocator::GetInput()
    {
        return *s_Input;
    }

    Time& ServiceLocator::GetTime()
    {
        return *s_Time;
    }

    IGraphicsContext& ServiceLocator::GetGraphicsContext()
    {
        return *s_GraphicsContext;
    }

    IRenderAPI& ServiceLocator::GetRenderAPI()
    {
        return *s_RenderAPI;
    }
}
