#pragma once

#include "core/InternedString.h"
#include "plugins/IPlugin.h"
#include "plugins/PluginDescriptor.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class AppBuilder;

    /**
     * @brief IPlugin registering SDLWindowSubsystem with the Presentation capability.
     */
    class WAYFINDER_API SDLWindowPlugin final : public IPlugin
    {
    public:
        void Build(AppBuilder& builder) override;

        [[nodiscard]] auto Describe() const -> PluginDescriptor override
        {
            return {.Name = InternedString::Intern("SDLWindowPlugin")};
        }
    };

    /**
     * @brief IPlugin registering SDLInputSubsystem (always active, no capability requirement).
     */
    class WAYFINDER_API SDLInputPlugin final : public IPlugin
    {
    public:
        void Build(AppBuilder& builder) override;

        [[nodiscard]] auto Describe() const -> PluginDescriptor override
        {
            return {.Name = InternedString::Intern("SDLInputPlugin")};
        }
    };

    /**
     * @brief IPlugin registering SDLTimeSubsystem (always active, no capability requirement).
     */
    class WAYFINDER_API SDLTimePlugin final : public IPlugin
    {
    public:
        void Build(AppBuilder& builder) override;

        [[nodiscard]] auto Describe() const -> PluginDescriptor override
        {
            return {.Name = InternedString::Intern("SDLTimePlugin")};
        }
    };

    /**
     * @brief PluginGroup expanding to SDLWindowPlugin + SDLInputPlugin + SDLTimePlugin.
     *
     * Convenience aggregate for registering all platform services at once.
     * Satisfies PluginGroupType (has Build(AppBuilder&), NOT derived from IPlugin).
     */
    struct WAYFINDER_API SDLPlatformPlugins
    {
        void Build(AppBuilder& builder);
    };

} // namespace Wayfinder
