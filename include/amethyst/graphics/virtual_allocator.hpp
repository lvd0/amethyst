#pragma once

#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>
#include <amethyst/meta/enums.hpp>

#include <vk_mem_alloc.h>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <memory>
#include <mutex>

namespace am {
    class CRawBuffer {
    public:
        using Self = CRawBuffer;
        struct SCreateInfo {
            EBufferUsage usage = {};
            uint64 capacity = 0;
            bool staging = false;
        };

        ~CRawBuffer() noexcept;

        AM_NODISCARD static std::unique_ptr<Self> make(CDevice*, SCreateInfo&&) noexcept;

        AM_NODISCARD VkBuffer native() const noexcept;
        AM_NODISCARD uint64 capacity() const noexcept;
        AM_NODISCARD uint64 alignment() const noexcept;
        AM_NODISCARD uint64 address() const noexcept;
        AM_NODISCARD uint8* data() noexcept;

        AM_NODISCARD SBufferInfo info(uint64 = 0) const noexcept;

    private:
        CRawBuffer() noexcept;

        VkBuffer _handle = {};
        VmaAllocation _allocation = {};
        uint64 _capacity = 0;
        uint64 _alignment = 0;
        uint64 _address = 0;
        void* _data = nullptr;

        CDevice* _device = nullptr;
    };

    class CBufferSlice {
    public:
        CBufferSlice() noexcept;
        CBufferSlice(CRawBuffer*, VmaVirtualAllocation, uint64, uint64) noexcept;

        AM_NODISCARD const CRawBuffer* handle() const noexcept;
        AM_NODISCARD uint64 size() const noexcept;
        AM_NODISCARD uint64 offset() const noexcept;
        AM_NODISCARD VmaVirtualAllocation virtual_allocation() const noexcept;

        void insert(const void*, uint64) noexcept;
        AM_NODISCARD SBufferInfo info(uint64 = 0) const noexcept;

    private:
        CRawBuffer* _handle = nullptr;
        VmaVirtualAllocation _virtual_alloc = {};
        uint64 _size = 0;
        uint64 _offset = 0;
    };

    class AM_MODULE CVirtualAllocator {
    public:
        using Self = CVirtualAllocator;

        ~CVirtualAllocator() noexcept;

        AM_NODISCARD static std::unique_ptr<Self> make(CDevice*, EBufferUsage, bool = false) noexcept;

        AM_NODISCARD CBufferSlice allocate(uint64, uint64 = 0) noexcept;
        void free(CBufferSlice&&) noexcept;

    private:
        struct SAllocationBlock {
            std::unique_ptr<CRawBuffer> buffer;
            VmaVirtualBlock block = {};
            uint64 allocations = 0;
        };

        CVirtualAllocator() noexcept;

        AM_NODISCARD SAllocationBlock _make_block() noexcept;
        AM_NODISCARD SAllocationBlock* _push_block() noexcept;
        AM_NODISCARD SAllocationBlock* _search_block(const CRawBuffer*);

        std::vector<SAllocationBlock> _blocks;
        EBufferUsage _usage = {};
        bool _staging = false;
        std::mutex _guard;

        CDevice* _device = nullptr;
    };
} // namespace am