#include "ServiceLocator.h"
#include "../platform/Input.h"
#include "../platform/Time.h"

namespace Wayfinder
{
    std::unique_ptr<Input> ServiceLocator::s_input = nullptr;
    std::unique_ptr<Time> ServiceLocator::s_time = nullptr;

    void ServiceLocator::Initialize(const BackendConfig& config)
    {
        s_input = Input::Create(config.Platform);
        s_time = Time::Create(config.Platform);
    }

    void ServiceLocator::Shutdown()
    {
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
}
