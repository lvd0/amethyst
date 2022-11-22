#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#include <string>

namespace am {
    class AM_MODULE CQueryPool : public IRefCounted {
    public:
        using Self = CQueryPool;
        struct SCreateInfo {
            EQueryPipelineStatistics statistics = {};
            EQueryType type = {};
            uint32 count = 0;
        };

        ~CQueryPool() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, SCreateInfo&&) noexcept;

        AM_NODISCARD VkQueryPool native() const noexcept;
        AM_NODISCARD uint64 count() const noexcept;

        AM_NODISCARD std::vector<uint64> results() const noexcept;

    private:
        CQueryPool() noexcept;

        VkQueryPool _handle = {};
        uint64 _count = 0;

        CRcPtr<CDevice> _device;
    };
} // namespace am
