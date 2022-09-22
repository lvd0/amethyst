#pragma once

#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

namespace am::prv {
    template <typename T>
    class AM_MODULE IDebugMarker {
    public:
        using Self = IDebugMarker;

        void set_debug_name(const char*) noexcept;

    protected:
        IDebugMarker() noexcept;
        ~IDebugMarker() noexcept;
    };

    template <typename T>
    IDebugMarker<T>::IDebugMarker() noexcept = default;

    template <typename T>
    IDebugMarker<T>::~IDebugMarker() noexcept = default;

    template <typename T>
    void IDebugMarker<T>::set_debug_name(const char* name) noexcept {
        static_cast<T*>(this)->_impl_set_debug_name(name);
    }
} // namespace am::prv
