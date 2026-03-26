#pragma once

namespace Wayfinder
{
    class ShaderProgramRegistry;

    /// Registers built-in shader programs used by the opaque forward path (`SceneOpaquePass`).
    void RegisterForwardOpaquePrograms(ShaderProgramRegistry& registry);

} // namespace Wayfinder
