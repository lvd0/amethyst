#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/image.hpp>

#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <amethyst/window/window.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#include <memory>

namespace am {
    class AM_MODULE CSwapchain : public IRefCounted {
    public:
        using Self = CSwapchain;
        struct SCreateInfo {
            EImageUsage usage =
                EImageUsage::ColorAttachment |
                EImageUsage::TransferDST;
            bool vsync = false;
            bool srgb = true;
        };

        ~CSwapchain() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, CRcPtr<CWindow>, SCreateInfo&&) noexcept;

        AM_NODISCARD VkSwapchainKHR native() const noexcept;
        AM_NODISCARD EResourceFormat format() const noexcept;
        AM_NODISCARD uint32 width() const noexcept;
        AM_NODISCARD uint32 height() const noexcept;
        AM_NODISCARD uint32 image_count() const noexcept;
        AM_NODISCARD const CImage* image(uint32) const noexcept;
        AM_NODISCARD bool is_lost() const noexcept;

        void set_lost() noexcept;
        void recreate() noexcept;
        void recreate(SCreateInfo&&) noexcept;

    private:
        static void _native_make(CDevice* device, CWindow* window, SCreateInfo&&, Self*) noexcept;

        CSwapchain() noexcept;

        VkSwapchainKHR _handle = {};
        VkSurfaceKHR _surface = {};
        VkFormat _format = {};
        uint32 _width = 0;
        uint32 _height = 0;
        std::vector<CRcPtr<CImage>> _images;
        bool _vsync = false;
        bool _srgb = false;
        bool _is_lost = false;

        CRcPtr<CDevice> _device;
        CRcPtr<CWindow> _window;
    };
} // namespace am
