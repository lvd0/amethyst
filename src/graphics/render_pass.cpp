#include <amethyst/graphics/render_pass.hpp>
#include <amethyst/graphics/context.hpp>

#include <optional>
#include <vector>

namespace am {
    CRenderPass::CRenderPass() noexcept = default;

    CRenderPass::~CRenderPass() noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(_device->logger(), "destroying render pass: {}", (const void*)_handle);
        vkDestroyRenderPass(_device->native(), _handle, nullptr);
    }

    AM_NODISCARD CRcPtr<CRenderPass> CRenderPass::make(CRcPtr<CDevice> device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(device->logger(), "building render pass");
        auto* result = new Self();
        std::vector<VkAttachmentDescription> attachment_descriptions;
        attachment_descriptions.reserve(info.attachments.size());
        for (const auto& each : info.attachments) {
            const auto is_stencil = prv::deduce_aspect(prv::as_vulkan(each.format.internal)) & VK_IMAGE_ASPECT_STENCIL_BIT;
            const auto is_depth = each.clear.type() == EClearValueType::Depth;
            const auto color_load =
                each.clear.type() == EClearValueType::None ?
                    VK_ATTACHMENT_LOAD_OP_LOAD :
                    VK_ATTACHMENT_LOAD_OP_CLEAR;
            const auto color_store =
                each.discard ?
                    VK_ATTACHMENT_STORE_OP_DONT_CARE :
                    VK_ATTACHMENT_STORE_OP_STORE;
            const auto stencil_load =
                is_depth && is_stencil ?
                    VK_ATTACHMENT_LOAD_OP_CLEAR :
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            const auto stencil_store =
                !is_stencil || each.discard ?
                    VK_ATTACHMENT_STORE_OP_DONT_CARE :
                    VK_ATTACHMENT_STORE_OP_STORE;
            VkAttachmentDescription description = {};
            description.format = prv::as_vulkan(
                each.format.view == EResourceFormat::Undefined ?
                    each.format.internal :
                    each.format.view);
            description.samples = prv::as_vulkan(each.samples);
            description.loadOp = color_load;
            description.storeOp = color_store;
            description.stencilLoadOp = stencil_load;
            description.stencilStoreOp = stencil_store;
            description.initialLayout = prv::as_vulkan(each.layout.initial);
            description.finalLayout = prv::as_vulkan(each.layout.final);
            attachment_descriptions.emplace_back(description);
        }
        result->_attachments = std::move(info.attachments);
        AM_LOG_INFO(device->logger(), "- attachment count: {}", attachment_descriptions.size());

        struct SSubpassStorage {
            std::vector<VkAttachmentReference> color;
            std::vector<VkAttachmentReference> input;
            std::optional<VkAttachmentReference> depth;
        };
        std::vector<VkSubpassDescription> subpasses;
        subpasses.reserve(info.subpasses.size());
        std::vector<SSubpassStorage> subpass_storage;
        subpass_storage.reserve(info.subpasses.size());
        for (const auto& subpass : info.subpasses) {
            auto& storage = subpass_storage.emplace_back();
            const auto process_attachments = [&](const std::vector<uint32>& attachments, bool is_input, bool check_depth) {
                std::vector<VkAttachmentReference> references;
                for (const auto& index : attachments) {
                    const auto aspect = prv::deduce_aspect(attachment_descriptions[index].format);
                    const auto is_depth = aspect & VK_IMAGE_ASPECT_DEPTH_BIT;
                    VkAttachmentReference reference = {};
                    reference.attachment = index;
                    if (is_input) {
                        reference.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    } else if (is_depth) {
                        reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    } else {
                        reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    }

                    AM_UNLIKELY_IF(check_depth && is_depth) {
                        storage.depth = reference;
                    } else {
                        references.emplace_back(reference);
                    }
                }
                return references;
            };
            storage.color = process_attachments(subpass.attachments, false, true);
            storage.input = process_attachments(subpass.input, true, false);

            VkSubpassDescription description = {};
            description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            description.inputAttachmentCount = (uint32)storage.input.size();
            description.pInputAttachments = storage.input.data();
            description.colorAttachmentCount = (uint32)storage.color.size();
            description.pColorAttachments = storage.color.data();
            AM_LIKELY_IF(storage.depth) {
                description.pDepthStencilAttachment = &*storage.depth;
            }
            description.preserveAttachmentCount = (uint32)subpass.preserve.size();
            description.pPreserveAttachments = subpass.preserve.data();
            subpasses.emplace_back(description);
        }
        AM_LOG_INFO(device->logger(), "- subpass count: {}", subpasses.size());

        std::vector<VkSubpassDependency> dependencies;
        dependencies.reserve(info.dependencies.size());
        for (const auto& each : info.dependencies) {
            VkSubpassDependency dependency = {};
            dependency.srcSubpass = each.source_subpass;
            dependency.dstSubpass = each.dest_subpass;
            dependency.srcStageMask = prv::as_vulkan(each.source_stage);
            dependency.dstStageMask = prv::as_vulkan(each.dest_stage);
            dependency.srcAccessMask = prv::as_vulkan(each.source_access);
            dependency.dstAccessMask = prv::as_vulkan(each.dest_access);
            dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            dependencies.emplace_back(dependency);
        }
        AM_LOG_INFO(device->logger(), "- subpass dependencies count: {}", dependencies.size());

        VkRenderPassCreateInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = (uint32)attachment_descriptions.size();
        render_pass_info.pAttachments = attachment_descriptions.data();
        render_pass_info.subpassCount = (uint32)subpasses.size();
        render_pass_info.pSubpasses = subpasses.data();
        render_pass_info.dependencyCount = (uint32)dependencies.size();
        render_pass_info.pDependencies = dependencies.data();
        AM_VULKAN_CHECK(device->logger(), vkCreateRenderPass(device->native(), &render_pass_info, nullptr, &result->_handle));

        result->_device = std::move(device);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD VkRenderPass CRenderPass::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD const SAttachmentDescription& CRenderPass::attachment(uint32 index) const noexcept {
        AM_PROFILE_SCOPED();
        return _attachments[index];
    }
} // namespace am
