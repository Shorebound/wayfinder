#pragma once
#include <cstddef>
#include <cstdint>

namespace Wayfinder
{
    using KeyCode = uint16_t;

    namespace Key
    {
        inline constexpr std::size_t STATE_COUNT = 512;

        enum : KeyCode
        {
            // Values match SDL3 scancodes (physical key positions)
            A = 4,
            B = 5,
            C = 6,
            D = 7,
            E = 8,
            F = 9,
            G = 10,
            H = 11,
            I = 12,
            J = 13,
            K = 14,
            L = 15,
            M = 16,
            N = 17,
            O = 18,
            P = 19,
            Q = 20,
            R = 21,
            S = 22,
            T = 23,
            U = 24,
            V = 25,
            W = 26,
            X = 27,
            Y = 28,
            Z = 29,

            D1 = 30,
            D2 = 31,
            D3 = 32,
            D4 = 33,
            D5 = 34,
            D6 = 35,
            D7 = 36,
            D8 = 37,
            D9 = 38,
            D0 = 39,

            Enter = 40,
            Escape = 41,
            Backspace = 42,
            Tab = 43,
            Space = 44,

            Minus = 45,
            Equal = 46,
            LeftBracket = 47,
            RightBracket = 48,
            Backslash = 49,

            Semicolon = 51,
            Apostrophe = 52,
            GraveAccent = 53,
            Comma = 54,
            Period = 55,
            Slash = 56,

            CapsLock = 57,

            /* Function keys */
            F1 = 58,
            F2 = 59,
            F3 = 60,
            F4 = 61,
            F5 = 62,
            F6 = 63,
            F7 = 64,
            F8 = 65,
            F9 = 66,
            F10 = 67,
            F11 = 68,
            F12 = 69,

            PrintScreen = 70,
            ScrollLock = 71,
            Pause = 72,
            Insert = 73,
            Home = 74,
            PageUp = 75,
            Delete = 76,
            End = 77,
            PageDown = 78,

            Right = 79,
            Left = 80,
            Down = 81,
            Up = 82,

            NumLock = 83,

            /* Keypad */
            KPDivide = 84,
            KPMultiply = 85,
            KPSubtract = 86,
            KPAdd = 87,
            KPEnter = 88,
            KP1 = 89,
            KP2 = 90,
            KP3 = 91,
            KP4 = 92,
            KP5 = 93,
            KP6 = 94,
            KP7 = 95,
            KP8 = 96,
            KP9 = 97,
            KP0 = 98,
            KPDecimal = 99,
            KPEqual = 103,

            F13 = 104,
            F14 = 105,
            F15 = 106,
            F16 = 107,
            F17 = 108,
            F18 = 109,
            F19 = 110,
            F20 = 111,
            F21 = 112,
            F22 = 113,
            F23 = 114,
            F24 = 115,

            Menu = 118,

            /* Modifiers */
            LeftControl = 224,
            LeftShift = 225,
            LeftAlt = 226,
            LeftSuper = 227,
            RightControl = 228,
            RightShift = 229,
            RightAlt = 230,
            RightSuper = 231,
        };

        constexpr bool IsValid(KeyCode key)
        {
            return static_cast<std::size_t>(key) < STATE_COUNT;
        }
    }
}