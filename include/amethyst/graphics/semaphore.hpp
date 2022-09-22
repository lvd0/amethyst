#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/device.hpp>

#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#include <vector>

namespace am {
    class AM_MODULE CSemaphore : public IRefCounted {
    public:
        using Self = CSemaphore;

        ~CSemaphore() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>) noexcept;
        AM_NODISCARD static std::vector<CRcPtr<Self>> make(const CRcPtr<CDevice>&, uint32) noexcept;

        AM_NODISCARD VkSemaphore native() const noexcept;

        void wait() const noexcept;
        void signal() const noexcept;

    private:
        CSemaphore() noexcept;

        VkSemaphore _handle;

        CRcPtr<CDevice> _device;
    };
} // namespace am
