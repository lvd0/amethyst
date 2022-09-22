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
    class AM_MODULE CFence : public IRefCounted {
    public:
        using Self = CFence;

        ~CFence() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, bool = true) noexcept;
        AM_NODISCARD static std::vector<CRcPtr<Self>> make(const CRcPtr<CDevice>&, uint32, bool = true) noexcept;

        AM_NODISCARD VkFence native() const noexcept;

        AM_NODISCARD bool is_ready() const noexcept;
        void wait() const noexcept;
        void reset() const noexcept;
        void wait_and_reset() const noexcept;

    private:
        CFence() noexcept;

        VkFence _handle;

        CRcPtr<CDevice> _device;
    };

    class CThreadSafeFence : public IRefCounted {
    public:
        using Self = CThreadSafeFence;

        ~CThreadSafeFence() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, bool = true) noexcept;
        AM_NODISCARD static std::vector<CRcPtr<Self>> make(const CRcPtr<CDevice>&, uint32, bool = true) noexcept;

        AM_NODISCARD CFence* handle() const noexcept;

        AM_NODISCARD bool is_ready() const noexcept;
        void lock() const noexcept;
        void unlock() const noexcept;
        void wait() const noexcept;
        void reset() const noexcept;
        void wait_and_reset() const noexcept;

    private:
        CThreadSafeFence() noexcept;

        mutable std::mutex _mutex;

        CRcPtr<CFence> _fence;
    };
} // namespace am
