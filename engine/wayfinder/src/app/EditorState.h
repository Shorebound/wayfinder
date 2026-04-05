#pragma once

#include "IApplicationState.h"

namespace Wayfinder
{
    /**
     * @brief Non-gameplay IApplicationState stub for editor mode.
     *
     * Proves the IApplicationState pattern for non-gameplay states.
     * Provides an ImGui docking skeleton via DockSpaceOverViewport
     * when ImGui is available, otherwise a no-op.
     *
     * No Simulation dependency, no state subsystems.
     */
    class EditorState final : public IApplicationState
    {
    public:
        [[nodiscard]] auto OnEnter(EngineContext& context) -> Result<void> override;
        [[nodiscard]] auto OnExit(EngineContext& context) -> Result<void> override;

        void OnUpdate(EngineContext& context, float deltaTime) override;
        void OnRender(EngineContext& context) override;

        [[nodiscard]] auto GetBackgroundPreferences() const -> BackgroundMode override
        {
            return BackgroundMode::None;
        }

        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "EditorState";
        }
    };

} // namespace Wayfinder
