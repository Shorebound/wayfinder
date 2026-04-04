#pragma once

namespace Wayfinder
{
    /**
     * @brief Base interface for type-erased registrar storage in AppBuilder.
     *
     * Concrete registrars (State, System, Tag, Lifecycle, Config, Subsystem)
     * derive from this. The base only provides a virtual destructor for
     * unique_ptr<IRegistrar> cleanup. Each concrete registrar defines
     * its own typed Finalise() -> Result<OutputType>.
     */
    class IRegistrar
    {
    public:
        virtual ~IRegistrar() = default;
    };

} // namespace Wayfinder
