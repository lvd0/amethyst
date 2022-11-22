#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/query_pool.hpp>

namespace am {
    CQueryPool::CQueryPool() noexcept = default;

    CQueryPool::~CQueryPool() noexcept {
        AM_PROFILE_SCOPED();
        vkDestroyQueryPool(_device->native(), _handle, nullptr);
    }

    AM_NODISCARD CRcPtr<CQueryPool> CQueryPool::make(CRcPtr<CDevice> device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        VkQueryPoolCreateInfo query_pool_info = {};
        query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        query_pool_info.queryType = prv::as_vulkan(info.type);
        query_pool_info.queryCount = info.count;
        query_pool_info.pipelineStatistics = prv::as_vulkan(info.statistics);
        AM_VULKAN_CHECK(device->logger(), vkCreateQueryPool(device->native(), &query_pool_info, nullptr, &result->_handle));
        result->_count = info.count;
        result->_device = std::move(device);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD VkQueryPool CQueryPool::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD uint64 CQueryPool::count() const noexcept {
        AM_PROFILE_SCOPED();
        return _count;
    }

    AM_NODISCARD std::vector<uint64> CQueryPool::results() const noexcept {
        AM_PROFILE_SCOPED();
        std::vector<uint64> result;
        result.resize(_count);
        AM_VULKAN_CHECK(
            _device->logger(),
            vkGetQueryPoolResults(
                _device->native(),
                _handle,
                0,
                1,
                _count * sizeof(uint64),
                result.data(),
                sizeof(uint64),
                VK_QUERY_RESULT_64_BIT |
                VK_QUERY_RESULT_WAIT_BIT));
        return result;
    }
} // namespace am
