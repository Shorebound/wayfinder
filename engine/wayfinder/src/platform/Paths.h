#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace Wayfinder::Platform
{
    /// Resolves a path relative to the executable's base directory.
    /// Absolute paths are returned unchanged. Relative paths are joined with
    /// SDL_GetBasePath() (the directory containing the executable), not CWD,
    /// so IDE working directories don't break packaged assets next to the binary.
    [[nodiscard]] inline std::string ResolvePathFromBase(std::string_view path)
    {
        const std::filesystem::path dir(path);
        if (dir.is_absolute())
        {
            return dir.string();
        }
        if (const char* base = SDL_GetBasePath())
        {
            return (std::filesystem::path(base) / dir).lexically_normal().string();
        }
        return std::string(path);
    }

} // namespace Wayfinder::Platform
