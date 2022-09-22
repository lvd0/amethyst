#include <amethyst/graphics/image.hpp>
#include <amethyst/graphics/queue.hpp>

namespace am {
    CImage::CImage() noexcept = default;

    CImage::~CImage() noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(_device->logger(), "deallocating image: {}", (const void*)_handle);
        vkDestroyImageView(_device->native(), _view, nullptr);
        AM_LIKELY_IF(_owning) {
            vmaDestroyImage(_device->allocator(), _handle, _allocation);
        }
    }

    AM_NODISCARD CRcPtr<CImage> CImage::make(CRcPtr<CDevice> device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto result = new Self();
        const auto aspect = prv::deduce_aspect(prv::as_vulkan(info.format));
        result->_samples = prv::as_vulkan(info.samples);
        result->_aspect = aspect;
        result->_usage = prv::as_vulkan(info.usage);
        result->_format = prv::as_vulkan(info.format);
        result->_layers = info.layers;
        result->_mips = info.mips;
        result->_width = info.width;
        result->_height = info.height;
        result->_owning = true;

        uint32 family;
        switch (info.queue) {
            case EQueueType::Graphics:
                family = device->graphics_queue()->family();
                break;
            case EQueueType::Transfer:
                family = device->transfer_queue()->family();
                break;
            case EQueueType::Compute:
                family = device->compute_queue()->family();
                break;
        }
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = prv::as_vulkan(info.format);
        image_info.extent = { info.width, info.height, 1 };
        image_info.mipLevels = info.mips;
        image_info.arrayLayers = info.layers;
        image_info.samples = prv::as_vulkan(info.samples);
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = prv::as_vulkan(info.usage);
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.queueFamilyIndexCount = 1;
        image_info.pQueueFamilyIndices = &family;
        image_info.initialLayout = prv::as_vulkan(info.layout);

        VmaAllocationCreateInfo allocation_info = {};
        allocation_info.flags = {};
        allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
        allocation_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        allocation_info.priority = 1;
        AM_VULKAN_CHECK(
            device->logger(),
            vmaCreateImage(
                device->allocator(),
                &image_info,
                &allocation_info,
                &result->_handle,
                &result->_allocation,
                nullptr));
        AM_LOG_INFO(device->logger(), "allocating image ({}): {}x{}x{}", (const void*)result->_handle, info.width, info.height, info.layers);

        VkImageViewCreateInfo image_view_info = {};
        image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_info.image = result->_handle;
        image_view_info.viewType =
            info.layers == 1 ?
                VK_IMAGE_VIEW_TYPE_2D :
                VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        image_view_info.format = prv::as_vulkan(info.format);
        image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.subresourceRange.aspectMask = aspect;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = info.mips;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = info.layers;
        AM_VULKAN_CHECK(device->logger(), vkCreateImageView(device->native(), &image_view_info, nullptr, &result->_view));
        result->_device = std::move(device);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD CRcPtr<CImage> CImage::from_raw(CRcPtr<CDevice> device, SRawImage&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto result = new Self();
        result->_handle = info.handle;
        result->_view = info.view;
        result->_allocation = info.allocation;
        result->_usage = info.usage;
        result->_samples = info.samples;
        result->_aspect = info.aspect;
        result->_format = info.format;
        result->_layers = info.layers;
        result->_mips = info.mips;
        result->_width = info.width;
        result->_height = info.height;
        result->_owning = false;
        result->_device = std::move(device);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD VkImage CImage::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD VkImageView CImage::view() const noexcept {
        AM_PROFILE_SCOPED();
        return _view;
    }

    AM_NODISCARD VkImageUsageFlags CImage::usage() const noexcept {
        return _usage;
    }

    AM_NODISCARD VkSampleCountFlagBits CImage::samples() const noexcept {
        return _samples;
    }

    AM_NODISCARD uint32 CImage::aspect() const noexcept {
        AM_PROFILE_SCOPED();
        return _aspect;
    }

    AM_NODISCARD VkFormat CImage::format() const noexcept {
        return _format;
    }

    AM_NODISCARD uint32 CImage::width() const noexcept {
        AM_PROFILE_SCOPED();
        return _width;
    }

    AM_NODISCARD uint32 CImage::height() const noexcept {
        AM_PROFILE_SCOPED();
        return _height;
    }

    AM_NODISCARD uint32 CImage::layers() const noexcept {
        AM_PROFILE_SCOPED();
        return _layers;
    }

    AM_NODISCARD uint32 CImage::mips() const noexcept {
        AM_PROFILE_SCOPED();
        return _mips;
    }
} // namespace am
