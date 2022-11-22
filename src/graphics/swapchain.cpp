#include <amethyst/graphics/swapchain.hpp>
#include <amethyst/graphics/context.hpp>
#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/queue.hpp>

#include <amethyst/meta/constants.hpp>

#include <amethyst/window/window.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace am {
    CSwapchain::CSwapchain() noexcept = default;

    CSwapchain::~CSwapchain() noexcept {
        AM_PROFILE_SCOPED();
        for (auto& image : _images) {
            image.reset();
        }
        vkDestroySwapchainKHR(_device->native(), _handle, nullptr);
        if (_surface) {
            vkDestroySurfaceKHR(_device->context()->native(), _surface, nullptr);
        }
    }

    AM_NODISCARD CRcPtr<CSwapchain> CSwapchain::make(CRcPtr<CDevice> device, CRcPtr<CWindow> window, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        _native_make(device.get(), window.get(), std::move(info), result);
        result->_device = std::move(device);
        result->_window = std::move(window);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD VkSwapchainKHR CSwapchain::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD EResourceFormat CSwapchain::format() const noexcept {
        AM_PROFILE_SCOPED();
        return static_cast<EResourceFormat>(_format);
    }

    AM_NODISCARD uint32 CSwapchain::width() const noexcept {
        AM_PROFILE_SCOPED();
        return _width;
    }

    AM_NODISCARD uint32 CSwapchain::height() const noexcept {
        AM_PROFILE_SCOPED();
        return _height;
    }

    AM_NODISCARD uint32 CSwapchain::image_count() const noexcept {
        AM_PROFILE_SCOPED();
        return (uint32)_images.size();
    }

    AM_NODISCARD const CImage* CSwapchain::image(uint32 index) const noexcept {
        AM_PROFILE_SCOPED();
        return _images[index].get();
    }

    bool CSwapchain::is_lost() const noexcept {
        AM_PROFILE_SCOPED();
        return _is_lost;
    }

    void CSwapchain::set_lost() noexcept {
        AM_PROFILE_SCOPED();
        _is_lost = true;
    }

    void CSwapchain::recreate() noexcept {
        recreate({
            .vsync = _vsync,
            .srgb = _srgb
        });
        _is_lost = false;
    }

    void CSwapchain::recreate(SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        _native_make(_device.get(), _window.get(), {
            .usage = (EImageUsage)_images[0]->usage(),
            .vsync = info.vsync,
            .srgb = info.srgb
        }, this);
        _is_lost = false;
    }

    void CSwapchain::_native_make(CDevice* device, CWindow* window, SCreateInfo&& info, Self* result) noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(device->logger(), "initializing swapchain");
        if (!result->_surface) {
            AM_VULKAN_CHECK(device->logger(), glfwCreateWindowSurface(device->context()->native(), window->native(), nullptr, &result->_surface));
        }

        VkBool32 present_support;
        const auto family = device->graphics_queue()->family();
        AM_VULKAN_CHECK(device->logger(), vkGetPhysicalDeviceSurfaceSupportKHR(device->gpu(), family, result->_surface, &present_support));
        AM_ASSERT(present_support, "surface or family does not support presentation");

        AM_LOG_INFO(device->logger(), "retrieving surface capabilities");
        VkSurfaceCapabilitiesKHR capabilities;
        AM_VULKAN_CHECK(device->logger(), vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->gpu(), result->_surface, &capabilities));

        auto image_count = capabilities.minImageCount + 1;
        AM_UNLIKELY_IF(capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
            image_count = capabilities.maxImageCount;
        }
        AM_LOG_INFO(device->logger(), "- image count: {}", image_count);

        AM_LIKELY_IF(capabilities.currentExtent.width != (uint32)-1) {
            AM_UNLIKELY_IF(capabilities.currentExtent.width == 0) {
                AM_VULKAN_CHECK(device->logger(), vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->gpu(), result->_surface, &capabilities));
            }
            result->_width = capabilities.currentExtent.width;
            result->_height = capabilities.currentExtent.height;
        } else {
            result->_width = std::clamp(window->width(), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            result->_height = std::clamp(window->height(), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        AM_LOG_INFO(device->logger(), "- width: {}", result->_width);
        AM_LOG_INFO(device->logger(), "- height: {}", result->_height);

        uint32 format_count;
        AM_VULKAN_CHECK(device->logger(), vkGetPhysicalDeviceSurfaceFormatsKHR(device->gpu(), result->_surface, &format_count, nullptr));
        std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
        AM_VULKAN_CHECK(device->logger(), vkGetPhysicalDeviceSurfaceFormatsKHR(device->gpu(), result->_surface, &format_count, surface_formats.data()));
        auto format = surface_formats[0];
        auto req_format = VK_FORMAT_B8G8R8A8_UNORM;
        if (info.srgb) {
            req_format = VK_FORMAT_B8G8R8A8_SRGB;
        }
        for (const auto& each : surface_formats) {
            AM_LIKELY_IF(each.format == req_format && each.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                format = each;
                break;
            }
        }
        result->_format = format.format;

        VkSwapchainCreateInfoKHR swapchain_info = {};
        swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_info.surface = result->_surface;
        swapchain_info.minImageCount = image_count;
        swapchain_info.imageFormat = result->_format;
        swapchain_info.imageColorSpace = format.colorSpace;
        swapchain_info.imageExtent = { result->_width, result->_height };
        swapchain_info.imageArrayLayers = 1;
        swapchain_info.imageUsage = prv::as_vulkan(info.usage);
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_info.queueFamilyIndexCount = 1;
        swapchain_info.pQueueFamilyIndices = &family;
        swapchain_info.preTransform = capabilities.currentTransform;
        swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_info.presentMode = info.vsync ?
            VK_PRESENT_MODE_FIFO_KHR :
            VK_PRESENT_MODE_IMMEDIATE_KHR;
        swapchain_info.clipped = true;
        if (result->_handle) {
            swapchain_info.oldSwapchain = result->_handle;
            device->cleanup_after(
                frames_in_flight,
                [handle = result->_handle, images = std::move(result->_images)](const CDevice* device) mutable noexcept {
                    for (auto& image : images) {
                        image.reset();
                    }
                    vkDestroySwapchainKHR(device->native(), handle, nullptr);
                });
        }
        result->_vsync = info.vsync;
        AM_VULKAN_CHECK(device->logger(), vkCreateSwapchainKHR(device->native(), &swapchain_info, nullptr, &result->_handle));

        std::vector<VkImage> images;
        AM_VULKAN_CHECK(device->logger(), vkGetSwapchainImagesKHR(device->native(), result->_handle, &image_count, nullptr));
        images.resize(image_count);
        AM_VULKAN_CHECK(device->logger(), vkGetSwapchainImagesKHR(device->native(), result->_handle, &image_count, images.data()));

        VkImageViewCreateInfo image_view_info = {};
        image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_info.format = result->_format;
        image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        result->_images.reserve(images.size());
        for (const auto image : images) {
            SRawImage raw = {};
            raw.handle = image;
            raw.usage = swapchain_info.imageUsage;
            raw.samples = VK_SAMPLE_COUNT_1_BIT;
            raw.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
            raw.format = { result->_format };
            raw.layout = VK_IMAGE_LAYOUT_UNDEFINED;
            raw.layers = 1;
            raw.mips = 1;
            raw.width = result->_width;
            raw.height = result->_height;
            image_view_info.image = image;
            AM_VULKAN_CHECK(device->logger(), vkCreateImageView(device->native(), &image_view_info, nullptr, &raw.view));
            result->_images.emplace_back(CImage::from_raw(CRcPtr<CDevice>::make(device), std::move(raw)));
        }
    }
} // namespace am
