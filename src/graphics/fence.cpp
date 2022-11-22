#include <amethyst/graphics/fence.hpp>

namespace am {
    CFence::CFence() noexcept = default;

    CFence::~CFence() noexcept {
        AM_PROFILE_SCOPED();
        vkDestroyFence(_device->native(), _handle, nullptr);
    }

    AM_NODISCARD CRcPtr<CFence> CFence::make(CRcPtr<CDevice> device, bool signaled) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
        AM_VULKAN_CHECK(device->logger(), vkCreateFence(device->native(), &fence_info, nullptr, &result->_handle));
        result->_device = std::move(device);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD std::vector<CRcPtr<CFence>> CFence::make(const CRcPtr<CDevice>& device, uint32 count, bool signaled) noexcept {
        AM_PROFILE_SCOPED();
        std::vector<CRcPtr<Self>> result;
        result.reserve(count);
        for (uint32 i = 0; i < count; ++i) {
            result.emplace_back(make(device, signaled));
        }
        return result;
    }

    AM_NODISCARD VkFence CFence::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD bool CFence::is_ready() const noexcept {
        AM_PROFILE_SCOPED();
        return vkGetFenceStatus(_device->native(), _handle) == VK_SUCCESS;
    }

    void CFence::wait() const noexcept {
        AM_PROFILE_SCOPED();
        AM_VULKAN_CHECK(_device->logger(), vkWaitForFences(_device->native(), 1, &_handle, true, (uint64)-1));
    }

    void CFence::reset() const noexcept {
        AM_PROFILE_SCOPED();
        AM_VULKAN_CHECK(_device->logger(), vkResetFences(_device->native(), 1, &_handle));
    }

    void CFence::wait_and_reset() const noexcept {
        AM_PROFILE_SCOPED();
        wait();
        reset();
    }



    CThreadSafeFence::CThreadSafeFence() noexcept = default;

    CThreadSafeFence::~CThreadSafeFence() noexcept = default;

    AM_NODISCARD CRcPtr<CThreadSafeFence> CThreadSafeFence::make(CRcPtr<CDevice> device, bool signaled) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        result->_fence = CFence::make(std::move(device), signaled);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD std::vector<CRcPtr<CThreadSafeFence>> CThreadSafeFence::make(const CRcPtr<CDevice>& device, uint32 count, bool signaled) noexcept {
        AM_PROFILE_SCOPED();
        std::vector<CRcPtr<Self>> result;
        result.reserve(count);
        for (uint32 i = 0; i < count; ++i) {
            result.emplace_back(make(device, signaled));
        }
        return result;
    }

    AM_NODISCARD CFence* CThreadSafeFence::handle() const noexcept {
        AM_PROFILE_SCOPED();
        return _fence.get();
    }

    AM_NODISCARD bool CThreadSafeFence::is_ready() const noexcept {
        AM_PROFILE_SCOPED();
        std::lock_guard guard(_mutex);
        return _fence->is_ready();
    }

    void CThreadSafeFence::lock() const noexcept {
        AM_PROFILE_SCOPED();
        _mutex.lock();
    }

    void CThreadSafeFence::unlock() const noexcept {
        AM_PROFILE_SCOPED();
        _mutex.unlock();
    }

    void CThreadSafeFence::wait() const noexcept {
        AM_PROFILE_SCOPED();
        std::lock_guard guard(_mutex);
        _fence->wait();
    }

    void CThreadSafeFence::reset() const noexcept {
        AM_PROFILE_SCOPED();
        std::lock_guard guard(_mutex);
        _fence->reset();
    }

    void CThreadSafeFence::wait_and_reset() const noexcept {
        AM_PROFILE_SCOPED();
        std::lock_guard guard(_mutex);
        _fence->wait_and_reset();
    }
} // namespace am
