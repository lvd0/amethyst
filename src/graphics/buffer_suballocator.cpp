#include <amethyst/graphics/buffer_suballocator.hpp>

namespace am {
    constexpr auto suballocation_initial_capacity = 134'217'728; // 128MiB

    CBufferSuballocator::CBufferSuballocator() noexcept = default;

    CBufferSuballocator::~CBufferSuballocator() noexcept = default;

    AM_NODISCARD std::unique_ptr<CBufferSuballocator> CBufferSuballocator::make(CDevice* device, EBufferUsage usage) noexcept {
        AM_PROFILE_SCOPED();
        auto result = new Self();
        result->_usage = usage;
        result->_device = device;
        return std::unique_ptr<Self>(result);
    }

    AM_NODISCARD SBufferSlice CBufferSuballocator::allocate(uint64 bytes) noexcept {
        AM_PROFILE_SCOPED();
        std::lock_guard lock(_guard);
        auto* pool = _search_free_pool(bytes);
        const auto block = _search_free_block(pool, bytes);
        const auto allocation = _register_block_allocation(pool, block, bytes);
        return { pool->buffer.get(), allocation.offset, allocation.size };
    }

    void CBufferSuballocator::free(SBufferSlice&& buffer) noexcept {
        AM_PROFILE_SCOPED();
        std::lock_guard lock(_guard);
        uint32 pool = 0;
        for (uint32 index = 0; auto& each : _pools) {
            if (each.buffer.get() == buffer.handle) {
                pool = index;
                break;
            }
            index++;
        }
        _pools[pool].free_blocks.push_back({
            buffer.offset,
            buffer.size
        });
        _merge_adjacent_blocks(&_pools[pool]);
        const auto& block = _pools[pool].free_blocks[0];
        if (block.size == suballocation_initial_capacity) {
            _pools.erase(_pools.begin() + pool);
        }
    }

    AM_NODISCARD SBufferPool* CBufferSuballocator::_make_pool() noexcept {
        AM_PROFILE_SCOPED();
        auto* result = &_pools.emplace_back();
        result->buffer = CTypedBuffer<uint8>::make(CRcPtr<CDevice>::make(_device), {
            .usage = _usage,
            .memory = memory_auto,
            .capacity = suballocation_initial_capacity,
            .shared = true
        });
        result->free_blocks.reserve(1024);
        result->free_blocks.push_back({
            .offset = 0,
            .size = suballocation_initial_capacity
        });
        return result;
    }

    AM_NODISCARD SBufferPool* CBufferSuballocator::_search_free_pool(uint64 bytes) noexcept {
        AM_PROFILE_SCOPED();
        for (auto& pool : _pools) {
            for (const auto& block : pool.free_blocks) {
                AM_LIKELY_IF(block.size >= bytes) {
                    return &pool;
                }
            }
        }
        return _make_pool();
    }

    AM_NODISCARD uint64 CBufferSuballocator::_search_free_block(SBufferPool* pool, uint64 bytes) noexcept {
        AM_PROFILE_SCOPED();
        uint64 index = 0;
        auto min_diff = (uint64)-1;
        for (uint64 i = 0; auto& each : pool->free_blocks) {
            if (each.size >= bytes) {
                const auto diff = std::min(each.size - bytes, min_diff);
                if (diff < min_diff) {
                    min_diff = diff;
                    index = i;
                }
            }
            ++i;
        }
        return index;
    }

    AM_NODISCARD SAllocationBlock CBufferSuballocator::_register_block_allocation(SBufferPool* pool, uint64 index, uint64 bytes) noexcept {
        AM_PROFILE_SCOPED();
        auto& block = pool->free_blocks[index];
        const auto offset = block.offset;
        block.size -= bytes;
        block.offset += bytes;
        if (block.size == 0) {
            pool->free_blocks.erase(pool->free_blocks.begin() + index);
        }
        return { offset, bytes };
    }

    void CBufferSuballocator::_merge_adjacent_blocks(SBufferPool* pool) noexcept {
        AM_PROFILE_SCOPED();
        std::sort(pool->free_blocks.begin(), pool->free_blocks.end(), [](const auto& x, const auto& y) {
            return x.offset < y.offset;
        });

        for (uint32 i = 0; i < pool->free_blocks.size() - 1;) {
            auto& left_side = pool->free_blocks[i];
            const auto& right_side = pool->free_blocks[i + 1];
            if (left_side.size == right_side.offset) {
                left_side.size += right_side.size;
                pool->free_blocks.erase(pool->free_blocks.begin() + i + 1);
            } else {
                ++i;
            }
        }
    }
} // namespace am
