#include <amethyst/graphics/semaphore.hpp>

namespace am {
    CSemaphore::CSemaphore() noexcept = default;

    CSemaphore::~CSemaphore() noexcept {
        AM_PROFILE_SCOPED();
        vkDestroySemaphore(_device->native(), _handle, nullptr);
    }

    AM_NODISCARD CRcPtr<CSemaphore> CSemaphore::make(CRcPtr<CDevice> device) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        AM_VULKAN_CHECK(device->logger(), vkCreateSemaphore(device->native(), &semaphore_info, nullptr, &result->_handle));
        result->_device = std::move(device);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD std::vector<CRcPtr<CSemaphore>> CSemaphore::make(const CRcPtr<CDevice>& device, uint32 count) noexcept {
        AM_PROFILE_SCOPED();
        std::vector<CRcPtr<Self>> result;
        result.reserve(count);
        for (uint32 i = 0; i < count; ++i) {
            result.emplace_back(make(device));
        }
        return result;
    }

    AM_NODISCARD VkSemaphore CSemaphore::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    void CSemaphore::wait() const noexcept {
        AM_PROFILE_SCOPED();
        // TODO
    }

    void CSemaphore::signal() const noexcept {
        AM_PROFILE_SCOPED();
        // TODO
    }
} // namespace am
