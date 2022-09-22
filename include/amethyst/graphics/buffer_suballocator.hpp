#pragma once

#include <amethyst/graphics/typed_buffer.hpp>

#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <vector>
#include <memory>
#include <mutex>

namespace am {
    struct SAllocationBlock {
        uint64 offset = 0;
        uint64 size = 0;
    };

    struct SBufferSlice {
        CTypedBuffer<uint8>* handle = nullptr;
        uint64 offset = 0;
        uint64 size = 0;
    };

    struct SBufferPool {
        CRcPtr<CTypedBuffer<uint8>> buffer;
        std::vector<SAllocationBlock> free_blocks;
    };

    class CBufferSuballocator {
    public:
        using Self = CBufferSuballocator;

        ~CBufferSuballocator() noexcept;

        AM_NODISCARD static std::unique_ptr<Self> make(CDevice*, EBufferUsage) noexcept;

        AM_NODISCARD SBufferSlice allocate(uint64) noexcept;
        void free(SBufferSlice&&) noexcept;
    private:
        CBufferSuballocator() noexcept;

        AM_NODISCARD SBufferPool* _make_pool() noexcept;
        AM_NODISCARD SBufferPool* _search_free_pool(uint64) noexcept;
        AM_NODISCARD uint64 _search_free_block(SBufferPool*, uint64) noexcept;
        AM_NODISCARD SAllocationBlock _register_block_allocation(SBufferPool*, uint64, uint64) noexcept;
        void _merge_adjacent_blocks(SBufferPool*) noexcept;

        std::vector<SBufferPool> _pools;
        EBufferUsage _usage = {};
        std::mutex _guard;

        CDevice* _device = nullptr;
    };
} // namespace am