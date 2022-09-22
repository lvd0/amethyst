#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

namespace am {
    enum class Keyboard : uint32 {
        kSpace = 32,
        kApostrophe = 39,
        kComma = 44,
        kMinus = 45,
        kPeriod = 46,
        kSlash = 47,
        k0 = 48,
        k1 = 49,
        k2 = 50,
        k3 = 51,
        k4 = 52,
        k5 = 53,
        k6 = 54,
        k7 = 55,
        k8 = 56,
        k9 = 57,
        kSemicolon = 59,
        kEqual = 61,
        kA = 65,
        kB = 66,
        kC = 67,
        kD = 68,
        kE = 69,
        kF = 70,
        kG = 71,
        kH = 72,
        kI = 73,
        kJ = 74,
        kK = 75,
        kL = 76,
        kM = 77,
        kN = 78,
        kO = 79,
        kP = 80,
        kQ = 81,
        kR = 82,
        kS = 83,
        kT = 84,
        kU = 85,
        kV = 86,
        kW = 87,
        kX = 88,
        kY = 89,
        kZ = 90,
        kLeftBracket = 91,
        kBackslash = 92,
        kRightBracket = 93,
        kGraveAccent = 96,
        kWorld1 = 161,
        kWorld2 = 162,
        kEscape = 256,
        kEnter = 257,
        kTab = 258,
        kBackspace = 259,
        kInsert = 260,
        kDelete = 261,
        kRight = 262,
        kLeft = 263,
        kDown = 264,
        kUp = 265,
        kPageUp = 266,
        kPageDown = 267,
        kHome = 268,
        kEnd = 269,
        kCapsLock = 280,
        kScrollLock = 281,
        kNumLock = 282,
        kPrintScreen = 283,
        kPause = 284,
        kF1 = 290,
        kF2 = 291,
        kF3 = 292,
        kF4 = 293,
        kF5 = 294,
        kF6 = 295,
        kF7 = 296,
        kF8 = 297,
        kF9 = 298,
        kF10 = 299,
        kF11 = 300,
        kF12 = 301,
        kF13 = 302,
        kF14 = 303,
        kF15 = 304,
        kF16 = 305,
        kF17 = 306,
        kF18 = 307,
        kF19 = 308,
        kF20 = 309,
        kF21 = 310,
        kF22 = 311,
        kF23 = 312,
        kF24 = 313,
        kF25 = 314,
        kNum0 = 320,
        kNum1 = 321,
        kNum2 = 322,
        kNum3 = 323,
        kNum4 = 324,
        kNum5 = 325,
        kNum6 = 326,
        kNum7 = 327,
        kNum8 = 328,
        kNum9 = 329,
        kNumDecimal = 330,
        kNumDivide = 331,
        kNumMultiply = 332,
        kNumSubtract = 333,
        kNumAdd = 334,
        kNumEnter = 335,
        kNumEqual = 336,
        kLeftShift = 340,
        kLeftControl = 341,
        kLeftAlt = 342,
        kLeftSuper = 343,
        kRightShift = 344,
        kRightControl = 345,
        kRightAlt = 346,
        kRightSuper = 347,
        kMenu = 348,
        kMaxEnum
    };

    class AM_MODULE CInput {
    public:
        using Self = CInput;
        ~CInput() noexcept;

        AM_NODISCARD static std::unique_ptr<Self> make(CRcPtr<CWindow>) noexcept;

        AM_NODISCARD const CWindow* window() const noexcept;

        AM_NODISCARD bool is_key_pressed(Keyboard) const noexcept;
        AM_NODISCARD bool is_key_released(Keyboard) const noexcept;
        AM_NODISCARD bool is_key_pressed_once(Keyboard) const noexcept;
        AM_NODISCARD bool is_key_released_once(Keyboard) const noexcept;

        void capture() noexcept;

    private:
        CInput() noexcept;

        bool _old_keys[(uint32)Keyboard::kMaxEnum] = {};
        bool _keys[(uint32)Keyboard::kMaxEnum] = {};

        CRcPtr<CWindow> _window;
    };
} // namespace am
