#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <type_traits>
#include <memory>

namespace am {
    namespace prv {
        template <typename T>
        struct TDeduceErrorType { using Type = typename T::EErrorType; };

        template <typename T>
        struct TDeduceErrorType<CRcPtr<T>> { using Type = typename T::EErrorType; };

        template <typename T>
        using DeduceErrorType = typename TDeduceErrorType<T>::Type;

        template <typename T, typename = void>
        struct TCheckRcPtr : std::false_type {};

        template <typename T>
        struct TCheckRcPtr<T, std::enable_if_t<std::is_same_v<decltype(std::declval<T>().get()), typename T::BoxedType*>>> : std::true_type {};

        template <typename T>
        constexpr auto check_rc_ptr = TCheckRcPtr<T>::value;
    } // namespace am::prv

    template <typename T, typename E = prv::DeduceErrorType<T>>
    class CExpected {
    public:
        using Self = CExpected;
        using BoxedType = T;
        using ErrorType = E;

        constexpr CExpected() noexcept;
        constexpr CExpected(T&&) noexcept;
        constexpr CExpected(E&&) noexcept;
        template <typename... Args>
        constexpr CExpected(Args&&...) noexcept;
        AM_CONSTEXPR ~CExpected() noexcept;
        AM_QUALIFIED_DECLARE_COPY(CExpected, constexpr);
        AM_QUALIFIED_DECLARE_MOVE(CExpected, constexpr);

        AM_NODISCARD constexpr T& value() & noexcept;
        AM_NODISCARD constexpr E& error() & noexcept;

        AM_NODISCARD constexpr const T& value() const& noexcept;
        AM_NODISCARD constexpr const E& error() const& noexcept;

        AM_NODISCARD constexpr T&& value() && noexcept;
        AM_NODISCARD constexpr E&& error() && noexcept;

        AM_NODISCARD constexpr decltype(auto) operator ->() noexcept;
        AM_NODISCARD constexpr decltype(auto) operator ->() const noexcept;

        AM_NODISCARD constexpr explicit operator bool() const noexcept;
        AM_NODISCARD constexpr bool operator !() const noexcept;

    private:
        constexpr void _destroy() noexcept;

        std::aligned_union_t<0, T, E> _storage;
        enum {
            tag_none,
            tag_value,
            tag_error,
        } _tag = tag_none;
    };

    template <typename T, typename E>
    constexpr CExpected<T, E>::CExpected() noexcept = default;

    template <typename T, typename E>
    constexpr CExpected<T, E>::CExpected(T&& value) noexcept : _tag(tag_value) {
        new (&_storage) T(std::move(value));
    }

    template <typename T, typename E>
    constexpr CExpected<T, E>::CExpected(E&& error) noexcept : _tag(tag_error) {
        new (&_storage) E(std::move(error));
    }

    template <typename T, typename E>
    template <typename... Args>
    constexpr CExpected<T, E>::CExpected(Args&&... args) noexcept {
        new (&_storage) T(std::forward<Args>(args)...);
    }

    template <typename T, typename E>
    AM_CONSTEXPR CExpected<T, E>::~CExpected() noexcept {
        _destroy();
    }

    template <typename T, typename E>
    constexpr CExpected<T, E>::CExpected(const CExpected& other) noexcept {
        *this = other;
    }

    template <typename T, typename E>
    constexpr CExpected<T, E>& CExpected<T, E>::operator =(const CExpected& other) noexcept {
        AM_UNLIKELY_IF(this != &other) {
            _destroy();
            switch (_tag) {
                case tag_value:
                    new (&_storage) T(other.value());
                    break;
                case tag_error:
                    new (&_storage) E(other.error());
                    break;
                default: break;
            }
            _tag = other._tag;
        }
        return *this;
    }

    template <typename T, typename E>
    constexpr CExpected<T, E>::CExpected(CExpected&& other) noexcept {
        *this = std::move(other);
    }

    template <typename T, typename E>
    constexpr CExpected<T, E>& CExpected<T, E>::operator =(CExpected&& other) noexcept {
        _destroy();
        switch (other._tag) {
            case tag_value:
                new (&_storage) T(std::move(other.value()));
                break;
            case tag_error:
                new (&_storage) E(std::move(other.error()));
                break;
            default: break;
        }
        std::swap(_tag, other._tag);
        return *this;
    }

    template <typename T, typename E>
    AM_NODISCARD constexpr T& CExpected<T, E>::value() & noexcept {
        AM_ASSERT(_tag == tag_value, "value() request cannot be fulfilled");
        return *std::launder(reinterpret_cast<T*>(&_storage));
    }

    template <typename T, typename E>
    AM_NODISCARD constexpr E& CExpected<T, E>::error() & noexcept {
        AM_ASSERT(_tag == tag_error, "error() request cannot be fulfilled");
        return *std::launder(reinterpret_cast<E*>(&_storage));
    }

    template <typename T, typename E>
    AM_NODISCARD constexpr const T& CExpected<T, E>::value() const& noexcept {
        AM_ASSERT(_tag == tag_value, "value() request cannot be fulfilled");
        return *std::launder(reinterpret_cast<const T*>(&_storage));
    }

    template <typename T, typename E>
    AM_NODISCARD constexpr const E& CExpected<T, E>::error() const& noexcept {
        AM_ASSERT(_tag == tag_error, "error() request cannot be fulfilled");
        return *std::launder(reinterpret_cast<const E*>(&_storage));
    }

    template <typename T, typename E>
    AM_NODISCARD constexpr T&& CExpected<T, E>::value() && noexcept {
        AM_ASSERT(_tag == tag_value, "value() request cannot be fulfilled");
        return std::move(*std::launder(reinterpret_cast<T*>(&_storage)));
    }

    template <typename T, typename E>
    AM_NODISCARD constexpr E&& CExpected<T, E>::error() && noexcept {
        AM_ASSERT(_tag == tag_error, "error() request cannot be fulfilled");
        return std::move(*std::launder(reinterpret_cast<E*>(&_storage)));
    }

    template <typename T, typename E>
    AM_NODISCARD constexpr decltype(auto) CExpected<T, E>::operator ->() noexcept {
        AM_ASSERT(_tag == tag_value, "value() request cannot be fulfilled");
        if constexpr (prv::check_rc_ptr<T>) {
            return std::launder(reinterpret_cast<T*>(&_storage))->get();
        } else {
            return std::launder(reinterpret_cast<T*>(&_storage));
        }
    }

    template <typename T, typename E>
    AM_NODISCARD constexpr decltype(auto) CExpected<T, E>::operator ->() const noexcept {
        AM_ASSERT(_tag == tag_value, "value() request cannot be fulfilled");
        if constexpr (prv::check_rc_ptr<T>) {
            return std::launder(reinterpret_cast<const T*>(&_storage))->get();
        } else {
            return std::launder(reinterpret_cast<const T*>(&_storage));
        }
    }

    template <typename T, typename E>
    constexpr CExpected<T, E>::operator bool() const noexcept {
        return _tag == tag_value;
    }

    template <typename T, typename E>
    constexpr bool CExpected<T, E>::operator !() const noexcept {
        return _tag == tag_error;
    }

    template <typename T, typename E>
    constexpr void CExpected<T, E>::_destroy() noexcept {
        switch (_tag) {
            case tag_value: {
                if constexpr (!std::is_trivial_v<T>) {
                    value().~T();
                }
            } break;

            case tag_error: {
                if constexpr (!std::is_trivial_v<E>) {
                    error().~E();
                }
            } break;

            default: break;
        }
        _tag = tag_none;
    }
} // namespace am
