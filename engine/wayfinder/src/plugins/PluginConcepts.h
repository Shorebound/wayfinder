#pragma once

#include <concepts>

namespace Wayfinder
{
    class IPlugin;
    class AppBuilder;

    /// Matches types derived from IPlugin (the standard plugin interface).
    template<typename T>
    concept PluginType = std::derived_from<T, IPlugin>;

    /// Matches plugin groups: types with Build(AppBuilder&) that are NOT IPlugin-derived.
    /// Groups are convenience aggregates that register multiple plugins.
    template<typename T>
    concept PluginGroupType = requires(T group, AppBuilder& builder) {
        { group.Build(builder) } -> std::same_as<void>;
    } and not std::derived_from<T, IPlugin>;

} // namespace Wayfinder
