#include <amethyst/graphics/command_buffer.hpp>
#include <amethyst/graphics/descriptor_set.hpp>
#include <amethyst/graphics/render_pass.hpp>
#include <amethyst/graphics/framebuffer.hpp>
#include <amethyst/graphics/query_pool.hpp>
#include <amethyst/graphics/async_mesh.hpp>
#include <amethyst/graphics/pipeline.hpp>
#include <amethyst/graphics/image.hpp>

#include <amethyst/meta/constants.hpp>

namespace am {
    AM_NODISCARD static inline VkCommandPool get_command_pool(CDevice* device, const CCommandBuffer::SCreateInfo& info) noexcept {
        AM_PROFILE_SCOPED();
        CQueue* queue;
        switch (info.queue) {
            case EQueueType::Graphics:
                queue = device->graphics_queue();
                break;
            case EQueueType::Transfer:
                queue = device->transfer_queue();
                break;
            case EQueueType::Compute:
                queue = device->compute_queue();
                break;
            default: AM_UNREACHABLE();
        }
        VkCommandPool pool;
        switch (info.pool) {
            case ECommandPoolType::Main:
                pool = queue->main_pool();
                break;
            case ECommandPoolType::Transient:
                pool = queue->transient_pool(info.index);
                break;
            default: AM_UNREACHABLE();
        }
        return pool;
    }

    AM_NODISCARD static inline VkPipelineBindPoint deduce_bind_point(const CPipeline* pipeline) noexcept {
        switch (pipeline->type()) {
            case EPipelineType::Graphics: return VK_PIPELINE_BIND_POINT_GRAPHICS;
            case EPipelineType::Compute: return VK_PIPELINE_BIND_POINT_COMPUTE;
            case EPipelineType::RayTracing: return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
        }
        AM_UNREACHABLE();
    }

    CCommandBuffer::CCommandBuffer() noexcept = default;

    CCommandBuffer::~CCommandBuffer() noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(_device->logger(), "deallocating command buffer: {}", (const void*)_handle);
        vkFreeCommandBuffers(_device->native(), _pool, 1, &_handle);
    }

    AM_NODISCARD CRcPtr<CCommandBuffer> CCommandBuffer::make(CRcPtr<CDevice> device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        VkCommandBufferAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = get_command_pool(device.get(), info);
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;
        AM_VULKAN_CHECK(device->logger(), vkAllocateCommandBuffers(device->native(), &allocate_info, &result->_handle));
        AM_LOG_DEBUG(device->logger(), "allocating command buffer: {}", (const void*)result->_handle);
        result->_pool = allocate_info.commandPool;
        result->_device = std::move(device);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD std::vector<CRcPtr<CCommandBuffer>> CCommandBuffer::make(const CRcPtr<CDevice>& device, uint32 count, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        std::vector<CRcPtr<CCommandBuffer>> result;
        result.reserve(count);
        std::vector<VkCommandBuffer> allocated;
        allocated.resize(count);

        VkCommandBufferAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = get_command_pool(device.get(), info);
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = count;
        AM_VULKAN_CHECK(device->logger(), vkAllocateCommandBuffers(device->native(), &allocate_info, allocated.data()));
        for (uint32 i = 0; i < count; ++i) {
            auto& current = result.emplace_back(CRcPtr<Self>::make(new Self()));
            current->_handle = allocated[i];
            current->_pool = allocate_info.commandPool;
            current->_device = device;
        }
        AM_LOG_INFO(device->logger(), "allocating command buffer: {}, count: {}", (const void*)allocated[0], count);
        return result;
    }

    CCommandBuffer& CCommandBuffer::begin() noexcept {
        AM_PROFILE_SCOPED();
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        AM_VULKAN_CHECK(_device->logger(), vkBeginCommandBuffer(_handle, &begin_info));
        return *this;
    }

    CCommandBuffer& CCommandBuffer::begin_render_pass(const CFramebuffer* framebuffer, const CRenderPass* render_pass) noexcept {
        AM_PROFILE_SCOPED();
        _active_framebuffer = framebuffer;
        const auto clears = framebuffer->clears();
        VkRenderPassBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        begin_info.renderPass = render_pass ?
            render_pass->native() :
            framebuffer->render_pass()->native();
        begin_info.framebuffer = framebuffer->native();
        begin_info.renderArea = { {}, framebuffer->viewport() };
        begin_info.clearValueCount = (uint32)clears.size();
        begin_info.pClearValues = clears.data();
        vkCmdBeginRenderPass(_handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::bind_pipeline(const CPipeline* pipeline) noexcept {
        AM_PROFILE_SCOPED();
        _active_pipeline = pipeline;
        vkCmdBindPipeline(_handle, deduce_bind_point(pipeline), pipeline->native());
        return *this;
    }

    CCommandBuffer& CCommandBuffer::set_viewport() noexcept {
        AM_PROFILE_SCOPED();
        const auto fb_viewport = _active_framebuffer->viewport();
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = (float32)fb_viewport.width;
        viewport.height = (float32)fb_viewport.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(_handle, 0, 1, &viewport);
        return *this;
    }

    CCommandBuffer::Self& CCommandBuffer::set_viewport(SInvertedViewportTag) noexcept {
        AM_PROFILE_SCOPED();
        const auto fb_viewport = _active_framebuffer->viewport();
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = (float32)fb_viewport.height;
        viewport.width = (float32)fb_viewport.width;
        viewport.height = -(float32)fb_viewport.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(_handle, 0, 1, &viewport);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::set_scissor() noexcept {
        AM_PROFILE_SCOPED();
        const auto fb_viewport = _active_framebuffer->viewport();
        VkRect2D scissor = {};
        scissor.offset = {};
        scissor.extent = fb_viewport;
        vkCmdSetScissor(_handle, 0, 1, &scissor);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::bind_descriptor_set(const CDescriptorSet* set) noexcept {
        AM_PROFILE_SCOPED();
        const auto handle = set->native();
        vkCmdBindDescriptorSets(_handle, deduce_bind_point(_active_pipeline), _active_pipeline->main_layout(), set->index(), 1, &handle, 0, nullptr);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::bind_vertex_buffer(const SBufferInfo& buffer) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdBindVertexBuffers(_handle, 0, 1, &buffer.handle, &buffer.offset);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::bind_index_buffer(const SBufferInfo& buffer) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdBindIndexBuffer(_handle, buffer.handle, buffer.offset, VK_INDEX_TYPE_UINT32);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::push_constants(EShaderStage stage, const void* data, uint32 size) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdPushConstants(_handle, _active_pipeline->main_layout(), prv::as_vulkan(stage), 0, size, data);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::bind_mesh(const CAsyncMesh* mesh) noexcept {
        AM_PROFILE_SCOPED();
        bind_vertex_buffer(mesh->vertices()->info());
        bind_vertex_buffer(mesh->indices()->info());
        return *this;
    }

    CCommandBuffer& CCommandBuffer::draw(uint32 vertices, uint32 instances, uint32 first_vertex, uint32 first_instance) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdDraw(_handle, vertices, instances, first_vertex, first_instance);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::draw_indexed(uint32 indices, uint32 instances, uint32 first_index, int32 vertex_offset, uint32 first_instance) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdDrawIndexed(_handle, indices, instances, first_index, vertex_offset, first_instance);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::draw_indirect(const SBufferInfo& buffer, uint32 draw_count) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdDrawIndirect(_handle, buffer.handle, buffer.offset, draw_count, sizeof(SDrawCommandIndirect));
        return *this;
    }

    CCommandBuffer& CCommandBuffer::draw_indexed_indirect(const SBufferInfo& buffer, uint32 draw_count) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdDrawIndexedIndirect(_handle, buffer.handle, buffer.offset, draw_count, sizeof(SDrawCommandIndexedIndirect));
        return *this;
    }

    CCommandBuffer& CCommandBuffer::draw_indexed_indirect_count(const SBufferInfo& buffer, const SBufferInfo& draw_count, uint32 max_draw_count) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdDrawIndexedIndirectCount(
            _handle,
            buffer.handle,
            buffer.offset,
            draw_count.handle,
            draw_count.offset,
            max_draw_count,
            sizeof(SDrawCommandIndexedIndirect));
        return *this;
    }

    CCommandBuffer& CCommandBuffer::end_render_pass() noexcept {
        AM_PROFILE_SCOPED();
        _active_framebuffer = nullptr;
        _active_pipeline = nullptr;
        vkCmdEndRenderPass(_handle);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::dispatch(uint32 x, uint32 y, uint32 z) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdDispatch(_handle, x, y, z);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::copy_buffer(const SBufferInfo& source, const SBufferInfo& dest) noexcept {
        AM_PROFILE_SCOPED();
        VkBufferCopy region = {};
        region.srcOffset = source.offset;
        region.dstOffset = dest.offset;
        region.size = source.size;
        vkCmdCopyBuffer(_handle, source.handle, dest.handle, 1, &region);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::copy_buffer_to_image(const SBufferInfo& buffer, const CImage* image, uint32 mip) noexcept {
        AM_PROFILE_SCOPED();
        VkBufferImageCopy region = {};
        region.bufferOffset = buffer.offset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = image->aspect();
        region.imageSubresource.mipLevel = mip;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
            std::max(image->width() >> mip, 1u),
            std::max(image->height() >> mip, 1u),
            1
        };
        vkCmdCopyBufferToImage(_handle, buffer.handle, image->native(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::barrier(const SBufferMemoryBarrier& info) noexcept {
        AM_PROFILE_SCOPED();
        VkBufferMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = prv::as_vulkan(info.source_access);
        barrier.dstAccessMask = prv::as_vulkan(info.dest_access);
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = info.buffer.handle;
        barrier.offset = info.buffer.offset;
        barrier.size = info.buffer.size;
        vkCmdPipelineBarrier(
            _handle,
            prv::as_vulkan(info.source_stage),
            prv::as_vulkan(info.dest_stage),
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            1, &barrier,
            0, nullptr);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::barrier(const SImageMemoryBarrier& info) noexcept {
        AM_PROFILE_SCOPED();
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = prv::as_vulkan(info.source_access);
        barrier.dstAccessMask = prv::as_vulkan(info.dest_access);
        barrier.oldLayout = prv::as_vulkan(info.old_layout);
        barrier.newLayout = prv::as_vulkan(info.new_layout);
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = info.image->native();
        barrier.subresourceRange.aspectMask = info.image->aspect();
        AM_UNLIKELY_IF(info.mip == all_mips) {
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = info.image->mips();
        } else {
            barrier.subresourceRange.baseMipLevel = info.mip;
            barrier.subresourceRange.levelCount = 1;
        }
        AM_UNLIKELY_IF(info.layer == all_layers) {
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = info.image->layers();
        } else {
            barrier.subresourceRange.baseArrayLayer = info.layer;
            barrier.subresourceRange.layerCount = 1;
        }
        vkCmdPipelineBarrier(
            _handle,
            prv::as_vulkan(info.source_stage),
            prv::as_vulkan(info.dest_stage),
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::barrier(EPipelineStage source_stage, EPipelineStage dest_stage) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdPipelineBarrier(
            _handle,
            prv::as_vulkan(source_stage),
            prv::as_vulkan(dest_stage),
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            0, nullptr);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::memory_barrier(const SMemoryBarrier& info) noexcept {
        AM_PROFILE_SCOPED();
        VkMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = prv::as_vulkan(info.source_access);
        barrier.dstAccessMask = prv::as_vulkan(info.dest_access);
        vkCmdPipelineBarrier(
            _handle,
            prv::as_vulkan(info.source_stage),
            prv::as_vulkan(info.dest_stage),
            VK_DEPENDENCY_BY_REGION_BIT,
            1, &barrier,
            0, nullptr,
            0, nullptr);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::copy_image(const CImage* source, const CImage* dest) noexcept {
        AM_PROFILE_SCOPED();
        VkImageCopy region = {};
        region.srcSubresource.aspectMask = source->aspect();
        region.srcSubresource.mipLevel = 0;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.layerCount = source->layers();
        region.dstSubresource.aspectMask = dest->aspect();
        region.dstSubresource.mipLevel = 0;
        region.dstSubresource.baseArrayLayer = 0;
        region.dstSubresource.layerCount = dest->layers();
        region.extent = { source->width(), source->height(), 1 };
        vkCmdCopyImage(
            _handle,
            source->native(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dest->native(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::clear_image(const CImage* image, CClearValue&& clear) noexcept {
        AM_PROFILE_SCOPED();
        const auto native = clear.native();
        VkImageSubresourceRange range = {};
        range.aspectMask = image->aspect();
        range.baseMipLevel = 0;
        range.levelCount = image->mips();
        range.baseArrayLayer = 0;
        range.layerCount = image->layers();
        switch (clear.type()) {
            case EClearValueType::Color:
                vkCmdClearColorImage(
                    _handle,
                    image->native(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    &native.color,
                    1,
                    &range);
                break;
            case EClearValueType::Depth:
                vkCmdClearDepthStencilImage(
                    _handle,
                    image->native(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    &native.depthStencil,
                    1,
                    &range);
                break;
            default: break;
        }

        return *this;
    }

    CCommandBuffer& CCommandBuffer::transfer_ownership(const CQueue& source, const CQueue& dest, const SBufferMemoryBarrier& info) noexcept {
        AM_PROFILE_SCOPED();
        VkBufferMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = prv::as_vulkan(info.source_access);
        barrier.dstAccessMask = prv::as_vulkan(info.dest_access);
        barrier.srcQueueFamilyIndex = source.family();
        barrier.dstQueueFamilyIndex = dest.family();
        barrier.buffer = info.buffer.handle;
        barrier.offset = info.buffer.offset;
        barrier.size = info.buffer.size;
        vkCmdPipelineBarrier(
            _handle,
            prv::as_vulkan(info.source_stage),
            prv::as_vulkan(info.dest_stage),
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            1, &barrier,
            0, nullptr);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::transfer_ownership(const CQueue& source, const CQueue& dest, const SImageMemoryBarrier& info) noexcept {
        AM_PROFILE_SCOPED();
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = prv::as_vulkan(info.source_access);
        barrier.dstAccessMask = prv::as_vulkan(info.dest_access);
        barrier.oldLayout = prv::as_vulkan(info.old_layout);
        barrier.newLayout = prv::as_vulkan(info.new_layout);
        barrier.srcQueueFamilyIndex = source.family();
        barrier.dstQueueFamilyIndex = dest.family();
        barrier.image = info.image->native();
        barrier.subresourceRange.aspectMask = info.image->aspect();
        AM_UNLIKELY_IF(info.mip == all_mips) {
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = info.image->mips();
        } else {
            barrier.subresourceRange.baseMipLevel = info.mip;
            barrier.subresourceRange.levelCount = 1;
        }
        AM_UNLIKELY_IF(info.layer == all_layers) {
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = info.image->layers();
        } else {
            barrier.subresourceRange.baseArrayLayer = info.layer;
            barrier.subresourceRange.layerCount = 1;
        }
        vkCmdPipelineBarrier(
            _handle,
            prv::as_vulkan(info.source_stage),
            prv::as_vulkan(info.dest_stage),
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::transition_layout(const SImageMemoryBarrier& info) noexcept {
        AM_PROFILE_SCOPED();
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = prv::as_vulkan(info.source_access);
        barrier.dstAccessMask = prv::as_vulkan(info.dest_access);
        barrier.oldLayout = prv::as_vulkan(info.old_layout);
        barrier.newLayout = prv::as_vulkan(info.new_layout);
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = info.image->native();
        barrier.subresourceRange.aspectMask = info.image->aspect();
        AM_UNLIKELY_IF(info.mip == all_mips) {
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = info.image->mips();
        } else {
            barrier.subresourceRange.baseMipLevel = info.mip;
            barrier.subresourceRange.levelCount = 1;
        }
        AM_UNLIKELY_IF(info.layer == all_layers) {
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = info.image->layers();
        } else {
            barrier.subresourceRange.baseArrayLayer = info.layer;
            barrier.subresourceRange.layerCount = 1;
        }
        vkCmdPipelineBarrier(
            _handle,
            prv::as_vulkan(info.source_stage),
            prv::as_vulkan(info.dest_stage),
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::set_checkpoint(const char* name) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdSetCheckpointNV(_handle, name);
        return *this;
    }

    CCommandBuffer& CCommandBuffer::end() noexcept {
        AM_PROFILE_SCOPED();
        AM_VULKAN_CHECK(_device->logger(), vkEndCommandBuffer(_handle));
        return *this;
    }

    CCommandBuffer& CCommandBuffer::begin_query(const CQueryPool* query, uint32 index) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdResetQueryPool(_handle, query->native(), index, (uint32)query->count());
        vkCmdBeginQuery(_handle, query->native(), index, {});
        return *this;
    }

    CCommandBuffer& CCommandBuffer::end_query(const CQueryPool* query, uint32 index) noexcept {
        AM_PROFILE_SCOPED();
        vkCmdEndQuery(_handle, query->native(), index);
        return *this;
    }

    AM_NODISCARD VkCommandBuffer CCommandBuffer::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD VkCommandPool CCommandBuffer::pool() const noexcept {
        AM_PROFILE_SCOPED();
        return _pool;
    }
} // namespace am
