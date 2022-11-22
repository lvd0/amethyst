#include <amethyst/graphics/framebuffer.hpp>

#include <amethyst/meta/constants.hpp>

namespace am {
    CFramebuffer::CFramebuffer() noexcept = default;

    CFramebuffer::~CFramebuffer() noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(_device->logger(), "destroying framebuffer: {}", (const void*)_handle);
        vkDestroyFramebuffer(_device->native(), _handle, nullptr);
    }

    AM_NODISCARD CRcPtr<CFramebuffer> CFramebuffer::make(CRcPtr<CDevice> device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        AM_LOG_INFO(device->logger(), "creating framebuffer with:");
        AM_LOG_INFO(device->logger(), "- references: {}", info.attachments.size());
        AM_LOG_INFO(device->logger(), "- render pass: {}", (const void*)info.pass->native());
        uint32 layers = {};
        std::vector<VkImageView> references;
        references.reserve(info.attachments.size());
        for (auto&& each : info.attachments) {
            AM_LIKELY_IF(each.is_owning) {
                const auto& attachment = each.attachment.info;
                const auto& description = info.pass->attachment(attachment.index);
                result->_images.push_back({
                    CImage::make(device, {
                        .queue = EQueueType::Graphics,
                        .samples = description.samples,
                        .usage = attachment.usage,
                        .format = {
                            description.format.internal,
                            description.format.view,
                        },
                        .layout = EImageLayout::Undefined,
                        .layers = attachment.layers,
                        .mips = attachment.mips,
                        .width = info.width,
                        .height = info.height
                    }).as_const(),
                    each.is_owning
                });
                const auto& [image, _] = result->_images.back();
                result->_clears.emplace_back(description.clear);
                references.emplace_back(image->view());
                result->_viewport = { image->width(), image->height() };
                layers = image->layers();
            } else {
                const auto& attachment = each.attachment.reference;
                const auto& description = info.pass->attachment(attachment.index);
                result->_images.push_back({ CRcPtr<const CImage>::make(attachment.image), false });
                result->_clears.emplace_back(description.clear);
                const auto& [image, _] = result->_images.back();
                references.emplace_back(image->view());
                result->_viewport = { image->width(), image->height() };
                layers = image->layers();
            }
        }

        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = info.pass->native();
        framebuffer_info.attachmentCount = (uint32)references.size();
        framebuffer_info.pAttachments = references.data();
        framebuffer_info.width = result->_viewport.width;
        framebuffer_info.height = result->_viewport.height;
        framebuffer_info.layers = layers;
        AM_VULKAN_CHECK(device->logger(), vkCreateFramebuffer(device->native(), &framebuffer_info, nullptr, &result->_handle));

        result->_device = std::move(device);
        result->_pass = std::move(info.pass);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD VkFramebuffer CFramebuffer::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD VkExtent2D CFramebuffer::viewport() const noexcept {
        AM_PROFILE_SCOPED();
        return _viewport;
    }

    AM_NODISCARD std::vector<VkClearValue> CFramebuffer::clears() const noexcept {
        AM_PROFILE_SCOPED();
        std::vector<VkClearValue> result;
        result.reserve(_clears.size());
        for (const auto& each : _clears) {
            result.emplace_back(each.native());
        }
        return result;
    }

    AM_NODISCARD const CImage* CFramebuffer::image(uint32 index) const noexcept {
        AM_PROFILE_SCOPED();
        return _images[index].image.get();
    }

    AM_NODISCARD const CRenderPass* CFramebuffer::render_pass() const noexcept {
        AM_PROFILE_SCOPED();
        return _pass.get();
    }

    bool CFramebuffer::resize(uint32 width, uint32 height) noexcept {
        AM_PROFILE_SCOPED();
        if (_viewport.width == width && _viewport.height == height) {
            return false;
        }
        std::vector<CRcPtr<const CImage>> old_images;
        for (auto& [image, is_owning] : _images) {
            auto&& old = std::move(image);
            if (is_owning) {
                auto new_image = CImage::make(_device, {
                    .queue = EQueueType::Graphics,
                    .samples = (EImageSampleCount)old->samples(),
                    .usage = (EImageUsage)old->usage(),
                    .format = {
                        (EResourceFormat)old->internal_format(),
                        (EResourceFormat)old->view_format(),
                    },
                    .layout = EImageLayout::Undefined,
                    .layers = old->layers(),
                    .mips = old->mips(),
                    .width = width,
                    .height = height
                }).as_const();
                old_images.emplace_back(std::move(old));
                image = std::move(new_image);
            }
        }
        _viewport = { width, height };
        uint32 layers = 0;
        std::vector<VkImageView> references;
        references.reserve(_images.size());
        for (const auto& [image, _] : _images) {
            references.emplace_back(image->view());
            layers = image->layers();
        }
        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = _pass->native();
        framebuffer_info.attachmentCount = (uint32)references.size();
        framebuffer_info.pAttachments = references.data();
        framebuffer_info.width = _viewport.width;
        framebuffer_info.height = _viewport.height;
        framebuffer_info.layers = layers;
        _device->cleanup_after(
            frames_in_flight,
            [handle = _handle, images = std::move(old_images)](const CDevice* device) mutable noexcept {
                for (auto& image : images) {
                    image.reset();
                }
                vkDestroyFramebuffer(device->native(), handle, nullptr);
            });
        AM_VULKAN_CHECK(_device->logger(), vkCreateFramebuffer(_device->native(), &framebuffer_info, nullptr, &_handle));
        return true;
    }

    void CFramebuffer::update_attachment(uint32 index, const CImage* image) noexcept {
        AM_PROFILE_SCOPED();
        _images[index].image = CRcPtr<const CImage>::make(image);
    }

    AM_NODISCARD SFramebufferAttachment make_attachment(SFramebufferAttachmentInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        SFramebufferAttachment result = {};
        result.attachment.info = info;
        result.is_owning = true;
        return result;
    }

    AM_NODISCARD SFramebufferAttachment make_attachment(SFramebufferReferenceInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        SFramebufferAttachment result = {};
        result.attachment.reference = info;
        result.is_owning = false;
        return result;
    }
} // namespace am
