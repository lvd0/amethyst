#include <amethyst/graphics/descriptor_pool.hpp>

#include <array>

namespace am {
    CDescriptorPool::CDescriptorPool() noexcept = default;

    CDescriptorPool::~CDescriptorPool() noexcept {
        AM_PROFILE_SCOPED();
        for (const auto& each : _handles) {
            vkDestroyDescriptorPool(_device->native(), each._handle, nullptr);
        }
    }

    AM_NODISCARD CRcPtr<CDescriptorPool> CDescriptorPool::make(CRcPtr<CDevice> device) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        auto current = _native_make(device.get());
        AM_LOG_INFO(device->logger(), "creating descriptor pool: {}", (const void*)current._handle);
        result->_handles.emplace_back(current);
        result->_device = std::move(device);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD VkDescriptorPool CDescriptorPool::fetch_pool() noexcept {
        auto& current = _handles.back();
        AM_LIKELY_IF(current._capacity > 0) {
            --current._capacity;
            return current._handle;
        }
        AM_LOG_WARN(_device->logger(), "{} pool capacity reached", (const void*)current._handle);
        return _handles.emplace_back(_native_make(_device.get()))._handle;
    }

    AM_NODISCARD CDescriptorPool::SNativeInfo CDescriptorPool::_native_make(const CDevice* device) noexcept {
        AM_PROFILE_SCOPED();
        const auto descriptor_sizes = std::to_array<VkDescriptorPoolSize>({
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, std::min(device->limits().maxDescriptorSetUniformBuffers, 16384u) },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, std::min(device->limits().maxDescriptorSetStorageBuffers, 16384u) },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::min(device->limits().maxDescriptorSetStorageImages, 16384u) },
            { VK_DESCRIPTOR_TYPE_SAMPLER, std::min(device->limits().maxDescriptorSetSamplers, 16384u) },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, std::min(device->limits().maxDescriptorSetSampledImages, 16384u) },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::min(device->limits().maxDescriptorSetSampledImages, 16384u) }
        });
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.poolSizeCount = (uint32)descriptor_sizes.size();
        pool_info.pPoolSizes = descriptor_sizes.data();
        for (const auto& [_, count] : descriptor_sizes) {
            pool_info.maxSets += count;
        }
        SNativeInfo current = {};
        AM_VULKAN_CHECK(device->logger(), vkCreateDescriptorPool(device->native(), &pool_info, nullptr, &current._handle));
        current._capacity = pool_info.maxSets;
        return current;
    }
} // namespace am
