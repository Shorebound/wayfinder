#pragma once

/**
 * @brief Wrapper include for the flecs ECS library.
 *
 * MSVC's optimizer emits C4702 (unreachable code) when instantiating
 * flecs templates that use `if constexpr` with exhaustive returns
 * (flecs/addons/cpp/component.hpp). This is a known false positive —
 * the code is well-formed C++17. GCC and Clang do not warn.
 *
 * Because C4702 is a backend warning, `/external:W0` cannot suppress
 * it — the diagnostic is emitted at template instantiation in *our*
 * TUs, not during header parsing. A pragma push/pop around the
 * include is the standard workaround.
 *
 * All engine code should include this header instead of <flecs.h>.
 */

#ifdef WAYFINDER_COMPILER_MSVC
    #pragma warning(push)
    #pragma warning(disable : 4702) // unreachable code
#endif

#include <flecs.h>

#ifdef WAYFINDER_COMPILER_MSVC
    #pragma warning(pop)
#endif
