#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/queue.hpp>

#include <amethyst/meta/constants.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#include <vk_mem_alloc.h>

#include <memory>

namespace am {
    struct SRawImage {
        VkImage handle = {};
        VkImageView view = {};
        VmaAllocation allocation = {};
        VkImageUsageFlags usage = {};
        VkSampleCountFlagBits samples = {};
        VkImageAspectFlags aspect = {};
        struct {
            VkFormat internal;
            VkFormat view;
        } format = {};
        VkImageLayout layout = {};
        uint32 layers = 0;
        uint32 mips = 0;
        uint32 width = 0;
        uint32 height = 0;
    };

    class AM_MODULE CImage : public IRefCounted {
    public:
        using Self = CImage;
        struct SCreateInfo {
            EQueueType queue = EQueueType::Graphics;
            EImageSampleCount samples = EImageSampleCount::s1;
            EImageUsage usage = {};
            struct {
                EResourceFormat internal = {};
                EResourceFormat view = {};
            } format = {};
            EImageLayout layout = EImageLayout::Undefined;
            uint32 layers = 1;
            uint32 mips = 1;
            uint32 width = 0;
            uint32 height = 0;
        };

        ~CImage() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, SCreateInfo&&) noexcept;
        AM_NODISCARD static CRcPtr<Self> from_raw(CRcPtr<CDevice>, SRawImage&&) noexcept;

        AM_NODISCARD VkImage native() const noexcept;
        AM_NODISCARD VkImageView view() const noexcept;
        AM_NODISCARD VkImageUsageFlags usage() const noexcept;
        AM_NODISCARD VkSampleCountFlagBits samples() const noexcept;
        AM_NODISCARD VkImageAspectFlags aspect() const noexcept;
        AM_NODISCARD VkFormat internal_format() const noexcept;
        AM_NODISCARD VkFormat view_format() const noexcept;
        AM_NODISCARD uint32 width() const noexcept;
        AM_NODISCARD uint32 height() const noexcept;
        AM_NODISCARD uint32 layers() const noexcept;
        AM_NODISCARD uint32 mips() const noexcept;

    private:
        CImage() noexcept;

        VkImage _handle = {};
        VkImageView _view = {};
        VmaAllocation _allocation = {};
        VkImageUsageFlags _usage = {};
        VkSampleCountFlagBits _samples = {};
        VkImageAspectFlags _aspect = {};
        struct {
            VkFormat _internal;
            VkFormat _view;
        } _format = {};
        uint32 _layers = 0;
        uint32 _mips = 0;
        uint32 _width = 0;
        uint32 _height = 0;
        bool _owning = false;

        CRcPtr<CDevice> _device;
    };

    class AM_MODULE CImageView : public IRefCounted {
    public:
        using Self = CImageView;
        struct SCreateInfo {
            CRcPtr<const CImage> image;
            EResourceFormat format;
            uint32 mip = all_mips;
            uint32 layer = all_layers;
        };

        ~CImageView() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, SCreateInfo&&) noexcept;
        AM_NODISCARD static CRcPtr<Self> from_image(const CImage*) noexcept;

        AM_NODISCARD VkImageView native() const noexcept;
        AM_NODISCARD const CImage* image() const noexcept;

    private:
        CImageView() noexcept;

        VkImageView _handle = {};
        bool _owning = false;

        CRcPtr<const CImage> _image;
        CRcPtr<CDevice> _device;
    };
} // namespace am
