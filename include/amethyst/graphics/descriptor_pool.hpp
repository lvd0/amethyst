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
    class AM_MODULE CDescriptorPool : public IRefCounted {
    public:
        using Self = CDescriptorPool;

        ~CDescriptorPool() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>) noexcept;

        AM_NODISCARD VkDescriptorPool fetch_pool() noexcept;

    private:
        struct SNativeInfo {
            VkDescriptorPool _handle = {};
            uint64 _capacity = 0;
        };

        CDescriptorPool() noexcept;

        AM_NODISCARD static SNativeInfo _native_make(const CDevice*) noexcept;

        std::vector<SNativeInfo> _handles;

        CRcPtr<CDevice> _device;
    };
} // namespace am
