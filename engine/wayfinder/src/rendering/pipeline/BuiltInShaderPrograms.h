#pragma once

namespace Wayfinder
{
    class ShaderProgramRegistry;

    /// Registers built-in shader programs used by the forward rendering path.
    void RegisterBuiltInShaderPrograms(ShaderProgramRegistry& registry);

} // namespace Wayfinder
