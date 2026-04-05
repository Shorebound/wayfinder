#include "PerformanceOverlay.h"

#include "EngineContext.h"
#include "core/Log.h"

#ifdef WAYFINDER_HAS_IMGUI
#include <imgui.h>
#endif

namespace Wayfinder
{
    auto PerformanceOverlay::OnAttach(EngineContext& /*context*/) -> Result<void>
    {
        Log::Info(LogEngine, "PerformanceOverlay: OnAttach");
        m_accumSeconds = 0.0f;
        m_frameCount = 0;
        m_displayFps = 0.0f;
        m_displayMs = 0.0f;
        return {};
    }

    auto PerformanceOverlay::OnDetach(EngineContext& /*context*/) -> Result<void>
    {
        Log::Info(LogEngine, "PerformanceOverlay: OnDetach");
        return {};
    }

    void PerformanceOverlay::OnUpdate(EngineContext& /*context*/, const float deltaTime)
    {
        m_accumSeconds += deltaTime;
        ++m_frameCount;

        if (m_accumSeconds >= REFRESH_INTERVAL)
        {
            m_displayFps = static_cast<float>(m_frameCount) / m_accumSeconds;
            m_displayMs = (m_accumSeconds / static_cast<float>(m_frameCount)) * 1000.0f;

            m_accumSeconds = 0.0f;
            m_frameCount = 0;
        }
    }

    void PerformanceOverlay::OnRender(EngineContext& /*context*/)
    {
#ifdef WAYFINDER_HAS_IMGUI
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(180.0f, 60.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Performance", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
        ImGui::Text("%.0f fps", m_displayFps);
        ImGui::Text("%.2f ms", m_displayMs);
        ImGui::End();
#endif
        // In headless mode (no ImGui context), this is a no-op.
    }

} // namespace Wayfinder
