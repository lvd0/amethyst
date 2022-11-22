#include <amethyst/window/window.hpp>
#include <amethyst/window/input.hpp>

#include <GLFW/glfw3.h>

#include <cstring>

namespace am {
    CInput::CInput() noexcept = default;

    CInput::~CInput() noexcept = default;

    AM_NODISCARD std::unique_ptr<CInput> CInput::make(CRcPtr<CWindow> window) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        result->_window = std::move(window);
        return std::unique_ptr<Self>(result);
    }

    AM_NODISCARD const CWindow* CInput::window() const noexcept {
        AM_PROFILE_SCOPED();
        return _window.get();
    }

    AM_NODISCARD bool CInput::is_key_pressed(Keyboard key) const noexcept {
        AM_PROFILE_SCOPED();
        return _keys[(uint32)key] == GLFW_PRESS;
    }

    AM_NODISCARD bool CInput::is_key_released(Keyboard key) const noexcept {
        AM_PROFILE_SCOPED();
        return _keys[(uint32)key] == GLFW_RELEASE;
    }

    AM_NODISCARD bool CInput::is_key_pressed_once(Keyboard key) const noexcept {
        AM_PROFILE_SCOPED();
        return _keys[(uint32)key] == GLFW_PRESS && _old_keys[(uint32)key] == GLFW_RELEASE;
    }

    AM_NODISCARD bool CInput::is_key_released_once(Keyboard key) const noexcept {
        AM_PROFILE_SCOPED();
        return _keys[(uint32)key] == GLFW_RELEASE && _old_keys[(uint32)key] == GLFW_PRESS;
    }

    void CInput::capture() noexcept {
        AM_PROFILE_SCOPED();
        std::memcpy(_old_keys, _keys, sizeof _keys);
        constexpr auto all_keys = std::to_array({
            (uint32)Keyboard::kSpace,
            (uint32)Keyboard::kApostrophe,
            (uint32)Keyboard::kComma,
            (uint32)Keyboard::kMinus,
            (uint32)Keyboard::kPeriod,
            (uint32)Keyboard::kSlash,
            (uint32)Keyboard::k0,
            (uint32)Keyboard::k1,
            (uint32)Keyboard::k2,
            (uint32)Keyboard::k3,
            (uint32)Keyboard::k4,
            (uint32)Keyboard::k5,
            (uint32)Keyboard::k6,
            (uint32)Keyboard::k7,
            (uint32)Keyboard::k8,
            (uint32)Keyboard::k9,
            (uint32)Keyboard::kSemicolon,
            (uint32)Keyboard::kEqual,
            (uint32)Keyboard::kA,
            (uint32)Keyboard::kB,
            (uint32)Keyboard::kC,
            (uint32)Keyboard::kD,
            (uint32)Keyboard::kE,
            (uint32)Keyboard::kF,
            (uint32)Keyboard::kG,
            (uint32)Keyboard::kH,
            (uint32)Keyboard::kI,
            (uint32)Keyboard::kJ,
            (uint32)Keyboard::kK,
            (uint32)Keyboard::kL,
            (uint32)Keyboard::kM,
            (uint32)Keyboard::kN,
            (uint32)Keyboard::kO,
            (uint32)Keyboard::kP,
            (uint32)Keyboard::kQ,
            (uint32)Keyboard::kR,
            (uint32)Keyboard::kS,
            (uint32)Keyboard::kT,
            (uint32)Keyboard::kU,
            (uint32)Keyboard::kV,
            (uint32)Keyboard::kW,
            (uint32)Keyboard::kX,
            (uint32)Keyboard::kY,
            (uint32)Keyboard::kZ,
            (uint32)Keyboard::kLeftBracket,
            (uint32)Keyboard::kBackslash,
            (uint32)Keyboard::kRightBracket,
            (uint32)Keyboard::kGraveAccent,
            (uint32)Keyboard::kWorld1,
            (uint32)Keyboard::kWorld2,
            (uint32)Keyboard::kEscape,
            (uint32)Keyboard::kEnter,
            (uint32)Keyboard::kTab,
            (uint32)Keyboard::kBackspace,
            (uint32)Keyboard::kInsert,
            (uint32)Keyboard::kDelete,
            (uint32)Keyboard::kRight,
            (uint32)Keyboard::kLeft,
            (uint32)Keyboard::kDown,
            (uint32)Keyboard::kUp,
            (uint32)Keyboard::kPageUp,
            (uint32)Keyboard::kPageDown,
            (uint32)Keyboard::kHome,
            (uint32)Keyboard::kEnd,
            (uint32)Keyboard::kCapsLock,
            (uint32)Keyboard::kScrollLock,
            (uint32)Keyboard::kNumLock,
            (uint32)Keyboard::kPrintScreen,
            (uint32)Keyboard::kPause,
            (uint32)Keyboard::kF1,
            (uint32)Keyboard::kF2,
            (uint32)Keyboard::kF3,
            (uint32)Keyboard::kF4,
            (uint32)Keyboard::kF5,
            (uint32)Keyboard::kF6,
            (uint32)Keyboard::kF7,
            (uint32)Keyboard::kF8,
            (uint32)Keyboard::kF9,
            (uint32)Keyboard::kF10,
            (uint32)Keyboard::kF11,
            (uint32)Keyboard::kF12,
            (uint32)Keyboard::kF13,
            (uint32)Keyboard::kF14,
            (uint32)Keyboard::kF15,
            (uint32)Keyboard::kF16,
            (uint32)Keyboard::kF17,
            (uint32)Keyboard::kF18,
            (uint32)Keyboard::kF19,
            (uint32)Keyboard::kF20,
            (uint32)Keyboard::kF21,
            (uint32)Keyboard::kF22,
            (uint32)Keyboard::kF23,
            (uint32)Keyboard::kF24,
            (uint32)Keyboard::kF25,
            (uint32)Keyboard::kNum0,
            (uint32)Keyboard::kNum1,
            (uint32)Keyboard::kNum2,
            (uint32)Keyboard::kNum3,
            (uint32)Keyboard::kNum4,
            (uint32)Keyboard::kNum5,
            (uint32)Keyboard::kNum6,
            (uint32)Keyboard::kNum7,
            (uint32)Keyboard::kNum8,
            (uint32)Keyboard::kNum9,
            (uint32)Keyboard::kNumDecimal,
            (uint32)Keyboard::kNumDivide,
            (uint32)Keyboard::kNumMultiply,
            (uint32)Keyboard::kNumSubtract,
            (uint32)Keyboard::kNumAdd,
            (uint32)Keyboard::kNumEnter,
            (uint32)Keyboard::kNumEqual,
            (uint32)Keyboard::kLeftShift,
            (uint32)Keyboard::kLeftControl,
            (uint32)Keyboard::kLeftAlt,
            (uint32)Keyboard::kLeftSuper,
            (uint32)Keyboard::kRightShift,
            (uint32)Keyboard::kRightControl,
            (uint32)Keyboard::kRightAlt,
            (uint32)Keyboard::kRightSuper,
            (uint32)Keyboard::kMenu
        });
        for (const auto key : all_keys) {
            _keys[key] = glfwGetKey(_window->native(), key);
        }
    }
} // namespace am
