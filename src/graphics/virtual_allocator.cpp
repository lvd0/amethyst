#include <amethyst/graphics/virtual_allocator.hpp>
#include <amethyst/graphics/typed_buffer.hpp>
#include <amethyst/graphics/device.hpp>

namespace am {
    constexpr auto suballocation_initial_capacity = 134'217'728; // 128MiB

    CRawBuffer::CRawBuffer() noexcept = default;

    CRawBuffer::~CRawBuffer() noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(_device->logger(), "deallocating buffer: {}", (const void*)_handle);
        vmaDestroyBuffer(_device->allocator(), _handle, _allocation);
    }

    AM_NODISCARD std::unique_ptr<CRawBuffer> CRawBuffer::make(CDevice* device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto result = new Self();
        const uint32 queue_families[3] = {
            device->graphics_queue()->family(),
            device->transfer_queue()->family(),
            device->compute_queue()->family()
        };
        uint32 queue_families_count = 1;
        AM_LIKELY_IF(queue_families[0] != queue_families[1]) {
            queue_families_count++;
        }
        AM_LIKELY_IF(queue_families[0] != queue_families[2] &&
                     queue_families[1] != queue_families[2]) {
            queue_families_count++;
        }
        if (device->feature_support(EDeviceFeature::BufferDeviceAddress)) {
            info.usage |= EBufferUsage::ShaderDeviceAddress;
        }

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = info.capacity;
        buffer_info.usage = prv::as_vulkan(info.usage);
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        AM_UNLIKELY_IF(queue_families_count >= 2 && !info.staging) {
            buffer_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
            buffer_info.queueFamilyIndexCount = queue_families_count;
            buffer_info.pQueueFamilyIndices = queue_families;
        }

        EMemoryProperty memory = {};
        if (buffer_info.usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) {
            memory = EMemoryProperty::DeviceLocal;
        } else if (prv::as_underlying(info.usage & (EBufferUsage::UniformBuffer | EBufferUsage::StorageBuffer | EBufferUsage::IndirectBuffer))) {
            memory = EMemoryProperty::HostVisible | EMemoryProperty::HostCoherent | EMemoryProperty::DeviceLocal;
        } else if (info.staging) {
            memory = EMemoryProperty::HostVisible | EMemoryProperty::HostCoherent;
        }
        VmaAllocationInfo extra_info = {};
        VmaAllocationCreateInfo allocation_info = {};
        AM_LIKELY_IF(prv::as_underlying(memory & EMemoryProperty::HostVisible)) {
            allocation_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            memory |= EMemoryProperty::HostCached;
        }
        allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
        allocation_info.requiredFlags = prv::as_vulkan(memory);
        allocation_info.memoryTypeBits = {};
        AM_VULKAN_CHECK(
            device->logger(),
            vmaCreateBuffer(
                device->allocator(),
                &buffer_info,
                &allocation_info,
                &result->_handle,
                &result->_allocation,
                &extra_info));
        result->_capacity = info.capacity;
        result->_data = extra_info.pMappedData;
        VkMemoryRequirements memory_req = {};
        vkGetBufferMemoryRequirements(device->native(), result->_handle, &memory_req);
        result->_alignment = memory_req.alignment;
        AM_LIKELY_IF(device->feature_support(EDeviceFeature::BufferDeviceAddress)) {
            VkBufferDeviceAddressInfo device_address_info = {};
            device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            device_address_info.buffer = result->_handle;
            result->_address = vkGetBufferDeviceAddress(device->native(), &device_address_info);
        }
        result->_device = device;
        return std::unique_ptr<Self>(result);
    }

    AM_NODISCARD VkBuffer CRawBuffer::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD uint64 CRawBuffer::capacity() const noexcept {
        AM_PROFILE_SCOPED();
        return _capacity;
    }

    AM_NODISCARD uint64 CRawBuffer::alignment() const noexcept {
        AM_PROFILE_SCOPED();
        return _alignment;
    }

    AM_NODISCARD uint64 CRawBuffer::address() const noexcept {
        AM_PROFILE_SCOPED();
        return _address;
    }

    AM_NODISCARD uint8* CRawBuffer::data() noexcept {
        AM_PROFILE_SCOPED();
        return static_cast<uint8*>(_data);
    }

    AM_NODISCARD SBufferInfo CRawBuffer::info(uint64 offset) const noexcept {
        AM_PROFILE_SCOPED();
        return {
            _handle,
            offset,
            _capacity,
            _address
        };
    }

    CBufferSlice::CBufferSlice() noexcept = default;

    CBufferSlice::CBufferSlice(CRawBuffer* handle, VmaVirtualAllocation alloc, uint64 size, uint64 offset) noexcept
        : _handle(handle),
          _virtual_alloc(alloc),
          _size(size),
          _offset(offset) {
        AM_PROFILE_SCOPED();
    }

    AM_NODISCARD const CRawBuffer* CBufferSlice::handle() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD uint64 CBufferSlice::size() const noexcept {
        AM_PROFILE_SCOPED();
        return _size;
    }

    AM_NODISCARD uint64 CBufferSlice::offset() const noexcept {
        AM_PROFILE_SCOPED();
        return _offset;
    }

    AM_NODISCARD VmaVirtualAllocation CBufferSlice::virtual_allocation() const noexcept {
        AM_PROFILE_SCOPED();
        return _virtual_alloc;
    }

    void CBufferSlice::insert(const void* data, uint64 size) noexcept {
        AM_PROFILE_SCOPED();
        std::memcpy(_handle->data() + _offset, data, size);
    }

    AM_NODISCARD SBufferInfo CBufferSlice::info(uint64 offset) const noexcept {
        AM_PROFILE_SCOPED();
        return {
            _handle->native(),
            _offset + offset,
            _size,
            _handle->address()
        };
    }

    CVirtualAllocator::CVirtualAllocator() noexcept = default;

    CVirtualAllocator::~CVirtualAllocator() noexcept = default;

    AM_NODISCARD std::unique_ptr<CVirtualAllocator> CVirtualAllocator::make(CDevice* device, EBufferUsage usage, bool staging) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        result->_blocks.reserve(128);
        result->_usage = usage;
        result->_staging = staging;
        result->_device = device;
        return std::unique_ptr<Self>(result);
    }

    AM_NODISCARD CBufferSlice CVirtualAllocator::allocate(uint64 bytes, uint64 alignment) noexcept {
        AM_PROFILE_SCOPED();
        VkDeviceSize offset = 0;
        VmaVirtualAllocation allocation = {};
        VmaVirtualAllocationCreateInfo allocation_info = {};
        allocation_info.size = bytes;
        allocation_info.alignment = alignment;
        allocation_info.flags = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT;
        std::lock_guard lock(_guard);
        for (auto& each : _blocks) {
            AM_UNLIKELY_IF(!each.buffer) {
                each = _make_block();
            }
            auto& [buffer, block, allocations] = each;
            AM_UNLIKELY_IF(alignment == 0) {
                allocation_info.alignment = buffer->alignment();
            }
            bytes = align_size(bytes, allocation_info.alignment);
            auto result = vmaVirtualAllocate(block, &allocation_info, &allocation, &offset);
            AM_LIKELY_IF(result == VK_SUCCESS) {
                ++allocations;
                return {
                    buffer.get(),
                    allocation,
                    bytes,
                    offset
                };
            }
        }
        auto& [buffer, block, allocations] = *_push_block();
        AM_VULKAN_CHECK(_device->logger(), vmaVirtualAllocate(block, &allocation_info, &allocation, &offset));
        ++allocations;
        return {
            buffer.get(),
            allocation,
            bytes,
            offset
        };
    }

    void CVirtualAllocator::free(CBufferSlice&& buffer) noexcept {
        AM_PROFILE_SCOPED();
        std::lock_guard lock(_guard);
        auto* pool = _search_block(buffer.handle());
        vmaVirtualFree(pool->block, buffer.virtual_allocation());
        AM_UNLIKELY_IF(--pool->allocations == 0) {
            AM_UNLIKELY_IF(pool != &_blocks[0]) {
                std::swap(*pool, _blocks.back());
                _blocks.pop_back();
            }
        }
    }

    AM_NODISCARD CVirtualAllocator::SAllocationBlock CVirtualAllocator::_make_block() noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_WARN(_device->logger(), "allocating new virtual block: {} bytes", suballocation_initial_capacity);
        VmaVirtualBlockCreateInfo block_info = {};
        block_info.size = suballocation_initial_capacity;
        VmaVirtualBlock block = {};
        AM_VULKAN_CHECK(_device->logger(), vmaCreateVirtualBlock(&block_info, &block));
        auto buffer = CRawBuffer::make(_device, {
            .usage = _usage,
            .capacity = block_info.size,
            .staging = _staging
        });
        return { std::move(buffer), block };
    }

    AM_NODISCARD CVirtualAllocator::SAllocationBlock* CVirtualAllocator::_push_block() noexcept {
        return &_blocks.emplace_back(_make_block());
    }

    AM_NODISCARD CVirtualAllocator::SAllocationBlock* CVirtualAllocator::_search_block(const CRawBuffer* block) {
        AM_PROFILE_SCOPED();
        for (auto& each : _blocks) {
            if (each.buffer.get() == block) {
                return &each;
            }
        }
        return nullptr;
    }
} // namespace am
