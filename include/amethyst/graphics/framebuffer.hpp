#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/render_pass.hpp>
#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/image.hpp>

#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

namespace am {
    struct SFramebufferAttachmentInfo {
        uint32 index = 0;
        EImageUsage usage = {};
        uint32 layers = 1;
        uint32 mips = 1;
    };

    struct SFramebufferReferenceInfo {
        uint32 index = 0;
        const CImage* image = nullptr;
    };

    struct SFramebufferAttachment {
        union {
            SFramebufferAttachmentInfo info;
            SFramebufferReferenceInfo reference;
        } attachment;
        bool is_owning = false;
    };

    class AM_MODULE CFramebuffer : public IRefCounted {
    public:
        using Self = CFramebuffer;
        struct SCreateInfo {
            uint32 width;
            uint32 height;
            std::vector<SFramebufferAttachment> attachments;
            CRcPtr<CRenderPass> pass;
        };

        ~CFramebuffer() noexcept;

        AM_NODISCARD static CRcPtr<CFramebuffer> make(CRcPtr<CDevice>, SCreateInfo&&) noexcept;

        AM_NODISCARD VkFramebuffer native() const noexcept;
        AM_NODISCARD VkExtent2D viewport() const noexcept;
        AM_NODISCARD std::vector<VkClearValue> clears() const noexcept;
        AM_NODISCARD const CImage* image(uint32) const noexcept;
        AM_NODISCARD const CRenderPass* render_pass() const noexcept;

        bool resize(uint32, uint32) noexcept;
        void update_attachment(uint32, const CImage*) noexcept;

    private:
        struct SAttachment {
            CRcPtr<const CImage> image;
            bool is_owning = false;
        };

        CFramebuffer() noexcept;

        VkFramebuffer _handle = {};
        VkExtent2D _viewport = {};
        std::vector<CClearValue> _clears;
        std::vector<SAttachment> _images;

        CRcPtr<CDevice> _device;
        CRcPtr<CRenderPass> _pass;
    };

    AM_NODISCARD SFramebufferAttachment make_attachment(SFramebufferAttachmentInfo&&) noexcept;
    AM_NODISCARD SFramebufferAttachment make_attachment(SFramebufferReferenceInfo&&) noexcept;
} // namespace am
