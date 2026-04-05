#pragma once

#include "app/AppSubsystem.h"
#include "core/Result.h"
#include "wayfinder_exports.h"

#include <memory>

namespace Wayfinder
{
    class EngineContext;
    class Input;

    /**
     * @brief AppSubsystem wrapping the platform Input.
     *
     * Owns the Input instance and manages its lifecycle. Has no dependencies
     * on other subsystems and no required capabilities (always active).
     */
    class WAYFINDER_API InputSubsystem : public AppSubsystem
    {
    public:
        InputSubsystem() = default;
        ~InputSubsystem() override;

        [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
        void Shutdown() override;

        /// @pre Valid only after Initialise() and before Shutdown().
        [[nodiscard]] auto GetInput() -> Input&;

    private:
        std::unique_ptr<Input> m_input;
    };

} // namespace Wayfinder
