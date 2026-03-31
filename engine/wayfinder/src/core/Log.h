#pragma once
#include "core/logging/ILogger.h"
#include "core/logging/LogTypes.h"
#include "wayfinder_exports.h"

#include <cstdint>
#include <format>
#include <string_view>

namespace Wayfinder
{

    /**
     * @brief Lightweight, constexpr handle to a named log category.
     *
     * Lazily resolves to an ILogger& on first use, then caches the pointer.
     * The cache is invalidated when the logging system is reinitialised
     * (tracked via a generation counter).
     */
    class WAYFINDER_API LogCategoryHandle
    {
    public:
        constexpr explicit LogCategoryHandle(const std::string_view name) noexcept : m_name(name) {}

        ILogger& Get() const;

        std::string_view GetName() const
        {
            return m_name;
        }

    private:
        std::string_view m_name;
        mutable ILogger* m_cached = nullptr;
        mutable std::uint64_t m_generation = 0;
    };

    // ── Predefined categories ───────────────────────────────────────────

    extern WAYFINDER_API const LogCategoryHandle LogEngine;
    extern WAYFINDER_API const LogCategoryHandle LogRenderer;
    extern WAYFINDER_API const LogCategoryHandle LogInput;
    extern WAYFINDER_API const LogCategoryHandle LogAudio;
    extern WAYFINDER_API const LogCategoryHandle LogAssets;
    extern WAYFINDER_API const LogCategoryHandle LogPhysics;
    extern WAYFINDER_API const LogCategoryHandle LogGame;
    extern WAYFINDER_API const LogCategoryHandle LogScene;

    // ── Log namespace ───────────────────────────────────────────────────

    /**
     * @brief Engine logging API.
     *
     * Lifecycle:
     *   Log::Initialise();          // once at startup
     *   Log::Info(LogEngine, ...);  // anywhere
     *   Log::Shutdown();            // once at exit
     *
     * The template functions perform a verbosity check, then forward to the
     * logger's type-erased LogFormatted(). std::format_string gives
     * compile-time format validation.
     */
    namespace Log
    {
        WAYFINDER_API void Initialise();
        WAYFINDER_API void Shutdown();

        WAYFINDER_API ILogger& GetOrCreateLogger(std::string_view name, LogVerbosity defaultVerbosity = LogVerbosity::Info);
        WAYFINDER_API void SetGlobalVerbosity(LogVerbosity level);

        WAYFINDER_API const LogConfig& GetConfig();
        WAYFINDER_API void SetConfig(const LogConfig& config);

        // ── Typed log functions with compile-time format checking ────

        template<typename... TArgs>
        void Verbose(const LogCategoryHandle& cat, std::format_string<TArgs...> fmt, TArgs&&... args)
        {
            auto& logger = cat.Get();
            if (LogVerbosity::Verbose <= logger.GetVerbosity())
            {
                logger.LogFormatted(LogVerbosity::Verbose, fmt.get(), std::make_format_args(args...));
            }
        }

        template<typename... TArgs>
        void Info(const LogCategoryHandle& cat, std::format_string<TArgs...> fmt, TArgs&&... args)
        {
            auto& logger = cat.Get();
            if (LogVerbosity::Info <= logger.GetVerbosity())
            {
                logger.LogFormatted(LogVerbosity::Info, fmt.get(), std::make_format_args(args...));
            }
        }

        template<typename... TArgs>
        void Warn(const LogCategoryHandle& cat, std::format_string<TArgs...> fmt, TArgs&&... args)
        {
            auto& logger = cat.Get();
            if (LogVerbosity::Warning <= logger.GetVerbosity())
            {
                logger.LogFormatted(LogVerbosity::Warning, fmt.get(), std::make_format_args(args...));
            }
        }

        template<typename... TArgs>
        void Error(const LogCategoryHandle& cat, std::format_string<TArgs...> fmt, TArgs&&... args)
        {
            auto& logger = cat.Get();
            if (LogVerbosity::Error <= logger.GetVerbosity())
            {
                logger.LogFormatted(LogVerbosity::Error, fmt.get(), std::make_format_args(args...));
            }
        }

        template<typename... TArgs>
        void Fatal(const LogCategoryHandle& cat, std::format_string<TArgs...> fmt, TArgs&&... args)
        {
            auto& logger = cat.Get();
            if (LogVerbosity::Fatal <= logger.GetVerbosity())
            {
                logger.LogFormatted(LogVerbosity::Fatal, fmt.get(), std::make_format_args(args...));
            }
        }
    } // namespace Log

} // namespace Wayfinder