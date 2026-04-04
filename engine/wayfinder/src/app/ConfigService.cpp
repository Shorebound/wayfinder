#include "app/ConfigService.h"

namespace Wayfinder
{

    auto ConfigService::Initialise(EngineContext& /*context*/) -> Result<void>
    {
        return {};
    }

    void ConfigService::Shutdown()
    {
        m_configs.clear();
    }

} // namespace Wayfinder
