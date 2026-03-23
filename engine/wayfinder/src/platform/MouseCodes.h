#pragma once
#include <cstdint>

namespace Wayfinder
{
    using MouseCode = uint16_t;

    namespace Mouse
    {
        enum : MouseCode
        {
            // Values match SDL3 button constants
            ButtonLeft = 1,
            ButtonMiddle = 2,
            ButtonRight = 3,
            ButtonX1 = 4,
            ButtonX2 = 5,

            // Aliases
            Button0 = ButtonLeft,
            Button1 = ButtonMiddle,
            Button2 = ButtonRight,
            Button3 = ButtonX1,
            Button4 = ButtonX2,
            ButtonLast = ButtonX2,
        };
    }
}