#include "EditorState.h"

#include "EngineContext.h"
#include "core/Log.h"

#ifdef WAYFINDER_HAS_IMGUI
#include <imgui.h>
#endif

namespace Wayfinder
{
    auto EditorState::OnEnter(EngineContext& /*context*/) -> Result<void>
    {
        Log::Info(LogEngine, "EditorState: OnEnter");
        return {};
    }

    auto EditorState::OnExit(EngineContext& /*context*/) -> Result<void>
    {
        Log::Info(LogEngine, "EditorState: OnExit");
        return {};
    }

    void EditorState::OnUpdate(EngineContext& /*context*/, float /*deltaTime*/)
    {
        // Editor update logic would go here.
    }

    void EditorState::OnRender(EngineContext& /*context*/)
    {
#ifdef WAYFINDER_HAS_IMGUI
        // ImGui docking skeleton: creates the root docking space over the main viewport.
        // Child panels (scene view, inspector, etc.) are future editor work.
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
#endif
        // In headless mode or without ImGui, this is a no-op.
    }

} // namespace Wayfinder
