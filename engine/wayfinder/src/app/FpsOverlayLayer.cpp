#include "FpsOverlayLayer.h"

#include "platform/Window.h"

#include <format>

namespace Wayfinder
{
    namespace
    {
        /// How often to refresh the title (stable averages, low churn).
        constexpr float REFRESH_INTERVAL_SECONDS = 0.5f;
    } // namespace

    FpsOverlayLayer::FpsOverlayLayer(Window& window) : m_window(&window) {}

    void FpsOverlayLayer::OnAttach()
    {
        if (m_window)
        {
            m_baseTitle = m_window->GetTitle();
        }
        m_accumSeconds = 0.0f;
        m_framesInWindow = 0;
    }

    void FpsOverlayLayer::OnDetach()
    {
        if (m_window)
        {
            m_window->SetTitle(m_baseTitle);
        }
        m_window = nullptr;
    }

    void FpsOverlayLayer::OnUpdate(const float deltaTime)
    {
        if (!m_window || m_baseTitle.empty())
        {
            return;
        }

        m_accumSeconds += deltaTime;
        ++m_framesInWindow;

        if (m_accumSeconds < REFRESH_INTERVAL_SECONDS)
        {
            return;
        }

        const float avgMs = (m_accumSeconds / static_cast<float>(m_framesInWindow)) * 1000.0f;
        const float fps = static_cast<float>(m_framesInWindow) / m_accumSeconds;

        const std::string title = std::format("{} | {:.0f} fps · {:.2f} ms", m_baseTitle, fps, avgMs);
        m_window->SetTitle(title);

        m_accumSeconds = 0.0f;
        m_framesInWindow = 0;
    }

} // namespace Wayfinder
