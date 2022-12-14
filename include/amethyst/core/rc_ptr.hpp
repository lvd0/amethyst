#pragma once

#include <amethyst/core/ref_counted.hpp>

#include <utility>

namespace am {
    constexpr struct SDontGrab {} dont_grab;

    template <typename T>
    class AM_MODULE CRcPtr {
    public:
        using Self = CRcPtr;
        using BoxedType = T;

        CRcPtr() noexcept;
        CRcPtr(std::nullptr_t) noexcept;
        ~CRcPtr() noexcept;
        AM_DECLARE_COPY(CRcPtr);
        AM_DECLARE_MOVE(CRcPtr);

        AM_NODISCARD static Self make(T*) noexcept;
        AM_NODISCARD static Self make(T*, SDontGrab) noexcept;
        template <typename... Args>
        AM_NODISCARD static Self make(Args&&...) noexcept;

        template <typename U = std::add_const_t<T>>
        CRcPtr<U> as_const() const noexcept;

        void swap(CRcPtr<T>&) noexcept;
        void reset() noexcept;

        AM_NODISCARD T* get() const noexcept;
        AM_NODISCARD T* release() noexcept;

        AM_NODISCARD T& operator *() noexcept;
        AM_NODISCARD const T& operator *() const noexcept;

        AM_NODISCARD T* operator ->() const noexcept;

        AM_NODISCARD bool operator !() const noexcept;
        AM_NODISCARD explicit operator bool() const noexcept;

    private:
        T* _ptr = nullptr;
    };

    template <typename T>
    CRcPtr<T>::CRcPtr() noexcept = default;

    template <typename T>
    CRcPtr<T>::CRcPtr(std::nullptr_t) noexcept : _ptr(nullptr) {
        AM_PROFILE_SCOPED();
    }

    template <typename T>
    CRcPtr<T>::~CRcPtr() noexcept {
        AM_PROFILE_SCOPED();
        AM_LIKELY_IF(_ptr) {
            AM_UNLIKELY_IF(_ptr->drop() == 0) {
                delete _ptr;
            }
        }
    }

    template <typename T>
    CRcPtr<T>::CRcPtr(const CRcPtr& other) noexcept {
        AM_PROFILE_SCOPED();
        AM_UNLIKELY_IF(this != &other) {
            _ptr = other._ptr;
            AM_LIKELY_IF(_ptr) {
                _ptr->grab();
            }
        }
    }

    template <typename T>
    CRcPtr<T>& CRcPtr<T>::operator =(const CRcPtr& other) noexcept {
        AM_PROFILE_SCOPED();
        CRcPtr(other).swap(*this);
        return *this;
    }

    template <typename T>
    CRcPtr<T>::CRcPtr(CRcPtr&& other) noexcept {
        AM_PROFILE_SCOPED();
        using std::swap;
        swap(_ptr, other._ptr);
    }

    template <typename T>
    CRcPtr<T>& CRcPtr<T>::operator =(CRcPtr&& other) noexcept {
        AM_PROFILE_SCOPED();
        CRcPtr(std::move(other)).swap(*this);
        return *this;
    }

    template <typename T>
    AM_NODISCARD CRcPtr<T> CRcPtr<T>::make(T* ptr) noexcept {
        AM_PROFILE_SCOPED();
        Self result;
        result._ptr = ptr;
        AM_LIKELY_IF(ptr) {
            result._ptr->grab();
        }
        return result;
    }

    template <typename T>
    AM_NODISCARD CRcPtr<T> CRcPtr<T>::make(T* ptr, SDontGrab) noexcept {
        AM_PROFILE_SCOPED();
        Self result;
        result._ptr = ptr;
        return result;
    }

    template <typename T>
    template <typename... Args>
    AM_NODISCARD CRcPtr<T> CRcPtr<T>::make(Args&&... args) noexcept {
        AM_PROFILE_SCOPED();
        Self result;
        result._ptr = new T(std::forward<Args>(args)...);
        result._ptr->grab();
        return result;
    }

    template <typename T>
    template <typename U>
    CRcPtr<U> CRcPtr<T>::as_const() const noexcept {
        AM_PROFILE_SCOPED();
        return CRcPtr<U>::make(static_cast<U*>(_ptr));
    }

    template <typename T>
    void CRcPtr<T>::swap(CRcPtr<T>& other) noexcept {
        AM_PROFILE_SCOPED();
        using std::swap;
        swap(_ptr, other._ptr);
    }

    template <typename T>
    void CRcPtr<T>::reset() noexcept {
        AM_PROFILE_SCOPED();
        *this = Self();
    }

    template <typename T>
    AM_NODISCARD T* CRcPtr<T>::get() const noexcept {
        AM_PROFILE_SCOPED();
        return _ptr;
    }

    template <typename T>
    AM_NODISCARD T* CRcPtr<T>::release() noexcept {
        AM_PROFILE_SCOPED();
        AM_ASSERT(_ptr->drop() == 0, "release() only if refcount is 1");
        return std::exchange(_ptr, nullptr);
    }

    template <typename T>
    AM_NODISCARD T& CRcPtr<T>::operator *() noexcept {
        AM_PROFILE_SCOPED();
        return *_ptr;
    }

    template <typename T>
    AM_NODISCARD const T& CRcPtr<T>::operator *() const noexcept {
        AM_PROFILE_SCOPED();
        return *_ptr;
    }

    template <typename T>
    AM_NODISCARD T* CRcPtr<T>::operator ->() const noexcept {
        AM_PROFILE_SCOPED();
        return _ptr;
    }

    template <typename T>
    AM_NODISCARD bool CRcPtr<T>::operator !() const noexcept {
        AM_PROFILE_SCOPED();
        return !_ptr;
    }

    template <typename T>
    AM_NODISCARD CRcPtr<T>::operator bool() const noexcept {
        AM_PROFILE_SCOPED();
        return _ptr;
    }
} // namespace am
