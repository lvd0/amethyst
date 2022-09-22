#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/typed_buffer.hpp>
#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/queue.hpp>

#include <amethyst/meta/constants.hpp>
#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#include <vector>

namespace am {
    struct SMemoryBarrier {
        EPipelineStage source_stage = {};
        EPipelineStage dest_stage = {};
        EResourceAccess source_access = {};
        EResourceAccess dest_access = {};
    };

    struct SBufferMemoryBarrier {
        STypedBufferInfo buffer = {};
        EPipelineStage source_stage = {};
        EPipelineStage dest_stage = {};
        EResourceAccess source_access = {};
        EResourceAccess dest_access = {};
    };

    struct SImageMemoryBarrier {
        const CImage* image = nullptr;
        EPipelineStage source_stage = {};
        EPipelineStage dest_stage = {};
        EResourceAccess source_access = {};
        EResourceAccess dest_access = {};
        EImageLayout old_layout = {};
        EImageLayout new_layout = {};
        uint32 layer = all_layers;
        uint32 mip = all_mips;
    };

    struct SDrawCommandIndirect {
        uint32 vertices = 0;
        uint32 instances = 0;
        uint32 first_vertex = 0;
        uint32 first_instance = 0;
    };

    struct SDrawCommandIndexedIndirect {
        uint32 indices = 0;
        uint32 instances = 0;
        uint32 first_index = 0;
        int32 vertex_offset = 0;
        uint32 first_instance = 0;
    };

    class AM_MODULE CCommandBuffer : public IRefCounted {
    public:
        using Self = CCommandBuffer;
        struct SCreateInfo {
            EQueueType queue = {};
            ECommandPoolType pool = {};
            uint32 index = 0;
        };

        ~CCommandBuffer() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, SCreateInfo&&) noexcept;
        AM_NODISCARD static std::vector<CRcPtr<Self>> make(const CRcPtr<CDevice>&, uint32, SCreateInfo&&) noexcept;

        AM_NODISCARD VkCommandBuffer native() const noexcept;
        AM_NODISCARD VkCommandPool pool() const noexcept;

        Self& begin() noexcept;
        Self& begin_render_pass(const CFramebuffer*) noexcept;
        Self& bind_pipeline(const CPipeline*) noexcept;
        Self& set_viewport() noexcept;
        Self& set_viewport(SInvertedViewportTag) noexcept;
        Self& set_scissor() noexcept;
        Self& bind_descriptor_set(const CDescriptorSet*) noexcept;
        Self& bind_vertex_buffer(const STypedBufferInfo&) noexcept;
        Self& bind_index_buffer(const STypedBufferInfo&) noexcept;
        Self& push_constants(EShaderStage, const void*, uint32) noexcept;
        Self& draw(uint32, uint32, uint32, uint32) noexcept;
        Self& draw_indexed(uint32, uint32, uint32, int32, uint32) noexcept;
        Self& draw_indirect(const STypedBufferInfo&) noexcept;
        Self& draw_indexed_indirect(const STypedBufferInfo&) noexcept;
        Self& end_render_pass() noexcept;
        Self& dispatch(uint32 = 1, uint32 = 1, uint32 = 1) noexcept;
        Self& copy_buffer(const STypedBufferInfo&, const STypedBufferInfo&) noexcept;
        Self& copy_buffer_to_image(const STypedBufferInfo&, const CImage*, uint32 = 0) noexcept;
        Self& barrier(const SBufferMemoryBarrier&) noexcept;
        Self& barrier(const SImageMemoryBarrier&) noexcept;
        Self& barrier(EPipelineStage, EPipelineStage) noexcept;
        Self& memory_barrier(const SMemoryBarrier&) noexcept;
        Self& copy_image(const CImage*, const CImage*) noexcept;
        Self& transfer_ownership(const CQueue&, const CQueue&, const SBufferMemoryBarrier&) noexcept;
        Self& transfer_ownership(const CQueue&, const CQueue&, const SImageMemoryBarrier&) noexcept;
        Self& transition_layout(const SImageMemoryBarrier&) noexcept;
        Self& set_checkpoint(const char*) noexcept;
        Self& end() noexcept;

    private:
        CCommandBuffer() noexcept;

        VkCommandPool _pool = {};
        VkCommandBuffer _handle = {};

        const CFramebuffer* _active_framebuffer = nullptr;
        const CPipeline* _active_pipeline = nullptr;

        CRcPtr<CDevice> _device;
    };
} // namespace am
