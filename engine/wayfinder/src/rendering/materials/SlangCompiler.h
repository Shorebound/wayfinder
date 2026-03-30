#pragma once

#include "core/Result.h"
#include "rendering/backend/RenderDevice.h"
#include "wayfinder_exports.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Wayfinder
{

    /**
     * @brief Wraps the Slang C++ API for runtime .slang to SPIR-V compilation.
     *
     * Owns a Slang global session and compilation session configured for SPIR-V output.
     * Used by ShaderManager as a fallback when pre-compiled .spv files are not found
     * in Development/Debug builds.
     *
     * The Slang COM headers are confined to the .cpp via pimpl, so consumers
     * only need this header (no slang.h dependency).
     *
     * @note Shipping builds guard the runtime compilation callsite with
     *       `#if !defined(WAYFINDER_SHIPPING)`. A future issue could add explicit
     *       opt-in runtime compilation for Shipping (e.g. for modding) without
     *       architectural changes.
     */
    class WAYFINDER_API SlangCompiler
    {
    public:
        SlangCompiler();
        ~SlangCompiler();

        SlangCompiler(const SlangCompiler&) = delete;
        SlangCompiler& operator=(const SlangCompiler&) = delete;
        SlangCompiler(SlangCompiler&&) noexcept;
        SlangCompiler& operator=(SlangCompiler&&) noexcept;

        struct InitDesc
        {
            std::string_view SourceDirectory;
            std::span<const std::string> SearchPaths = {};
        };

        Result<void> Initialise(const InitDesc& desc);
        void Shutdown();

        struct CompileResult
        {
            std::vector<uint8_t> Bytecode;
        };

        /**
         * @brief Compile a .slang source file to SPIR-V for a specific entry point.
         * @param sourceName Stem name (e.g. "basic_lit") - resolved against sourceDirectory.
         * @param entryPoint Function name (e.g. "VSMain", "PSMain").
         * @param stage Shader stage (Vertex, Fragment, or Compute).
         * @return SPIR-V bytecode on success, or an Error with diagnostic details.
         */
        Result<CompileResult> Compile(std::string_view sourceName, std::string_view entryPoint, ShaderStage stage);

        /**
         * @brief Destroys and recreates the internal compilation session.
         *
         * Slang caches loaded modules inside its ISession, so editing an imported
         * module won't take effect until the session is recycled. Call this before
         * recompiling after source changes (e.g. during ReloadShaders).
         */
        Result<void> ResetSession();

        [[nodiscard]] bool IsInitialised() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
        std::string m_sourceDirectory;
        std::vector<std::string> m_searchPaths;
    };

} // namespace Wayfinder
