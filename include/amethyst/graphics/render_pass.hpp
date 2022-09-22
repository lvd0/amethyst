#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/clear_value.hpp>
#include <amethyst/graphics/device.hpp>

#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#include <vector>

namespace am {
    struct SAttachmentDescription {
        struct SLayout {
            EImageLayout initial = {};
            EImageLayout final = {};
        };
        EResourceFormat format = {};
        EImageSampleCount samples = {};
        SLayout layout = {};
        CClearValue clear;
        bool discard = false;
    };

    struct SSubpassDescription {
        std::vector<uint32> attachments;
        std::vector<uint32> preserve;
        std::vector<uint32> input;
    };

    struct SDependencyDescriptions {
        uint32 source_subpass = 0;
        uint32 dest_subpass = 0;
        EPipelineStage source_stage = {};
        EPipelineStage dest_stage = {};
        EResourceAccess source_access = {};
        EResourceAccess dest_access = {};
    };

    class AM_MODULE CRenderPass : public IRefCounted {
    public:
        using Self = CRenderPass;
        struct SCreateInfo {
            std::vector<SAttachmentDescription> attachments;
            std::vector<SSubpassDescription> subpasses;
            std::vector<SDependencyDescriptions> dependencies;
        };

        ~CRenderPass() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, SCreateInfo&&) noexcept;

        AM_NODISCARD VkRenderPass native() const noexcept;

        AM_NODISCARD const SAttachmentDescription& attachment(uint32) const noexcept;

    private:
        CRenderPass() noexcept;

        VkRenderPass _handle = {};
        std::vector<SAttachmentDescription> _attachments;

        CRcPtr<CDevice> _device;
    };
} // namespace am

