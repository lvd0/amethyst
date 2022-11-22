#pragma once

#include <amethyst/meta/macros.hpp>

#include <type_traits>
#include <cstdint>

#include <array>

namespace am {
    using int8 = std::int8_t;
    using int16 = std::int16_t;
    using int32 = std::int32_t;
    using int64 = std::int64_t;

    using uint8 = std::uint8_t;
    using uint16 = std::uint16_t;
    using uint32 = std::uint32_t;
    using uint64 = std::uint64_t;

    using float32 = float;
    using float64 = double;

    template <typename T>
    AM_NODISCARD constexpr uint64 size_bytes(const T& container) noexcept {
        return sizeof(*container.begin()) * container.size();
    }

    // Enum utilities
    template <typename T, typename U = std::underlying_type_t<T>, typename = std::enable_if_t<std::is_enum_v<T>>>
    AM_NODISCARD constexpr T operator ~(const T& value) noexcept {
        return static_cast<T>(~static_cast<U>(value));
    }

    template <typename T, typename U = std::underlying_type_t<T>, typename = std::enable_if_t<std::is_enum_v<T>>>
    AM_NODISCARD constexpr T operator |(const T& left, const T& right) noexcept {
        return static_cast<T>(static_cast<U>(left) | static_cast<U>(right));
    }

    template <typename T, typename U = std::underlying_type_t<T>, typename = std::enable_if_t<std::is_enum_v<T>>>
    AM_NODISCARD constexpr T operator &(const T& left, const T& right) noexcept {
        return static_cast<T>(static_cast<U>(left) & static_cast<U>(right));
    }

    template <typename T, typename U = std::underlying_type_t<T>, typename = std::enable_if_t<std::is_enum_v<T>>>
    constexpr T operator ^(const T& left, const T& right) noexcept {
        return static_cast<T>(static_cast<U>(left) ^ static_cast<U>(right));
    }

    template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
    constexpr T& operator |=(T& left, const T& right) noexcept {
        return left = static_cast<T>(left | right);
    }

    template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
    constexpr T& operator &=(T& left, const T& right) noexcept {
        return left = static_cast<T>(left & right);
    }

    template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
    constexpr T& operator ^=(T& left, const T& right) noexcept {
        return left = static_cast<T>(left ^ right);
    }
} // namespace am
