#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/queue.hpp>

#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#include <vk_mem_alloc.h>

#include <cstring>
#include <vector>

namespace am {
    struct STypedBufferInfo {
        VkBuffer handle = {};
        uint64 offset = 0;
        uint64 size = 0;
        uint64 address = 0;
    };

    struct SMemoryProperties {
        EMemoryProperty required = {};
        EMemoryProperty preferred = {};
    };

    template <typename T>
    class AM_MODULE CTypedBuffer : public IRefCounted {
    public:
        using Self = CTypedBuffer;
        using BoxedType = T;
        struct SCreateInfo {
            EBufferUsage usage = {};
            SMemoryProperties memory = {}; // Optional
            uint64 capacity = 0;
            bool staging = false;
            bool shared = false;
        };

        ~CTypedBuffer() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, SCreateInfo&&) noexcept;
        AM_NODISCARD static std::vector<CRcPtr<Self>> make(const CRcPtr<CDevice>&, uint32, SCreateInfo&&) noexcept;

        AM_NODISCARD VkBuffer native() const noexcept;
        AM_NODISCARD VkBufferUsageFlags buffer_usage() const noexcept;
        AM_NODISCARD VkMemoryPropertyFlags memory_usage() const noexcept;
        AM_NODISCARD uint64 size() const noexcept;
        AM_NODISCARD uint64 capacity() const noexcept;
        AM_NODISCARD uint64 address() const noexcept;

        AM_NODISCARD bool empty() const noexcept;
        AM_NODISCARD STypedBufferInfo info(uint64 = 0) const noexcept;
        AM_NODISCARD uint64 size_bytes() const noexcept;

        AM_NODISCARD void* raw() noexcept;
        AM_NODISCARD const void* raw() const noexcept;

        AM_NODISCARD T* data() noexcept;
        AM_NODISCARD const T* data() const noexcept;

        AM_NODISCARD T& operator [](uint64) noexcept;
        AM_NODISCARD const T& operator [](uint64) const noexcept;

        void insert(const T&) noexcept;
        void insert(uint64, const T&) noexcept;
        void insert(const void*, uint64) noexcept;
        void insert(const std::vector<T>&) noexcept;
        void insert(uint64, const std::vector<T>&) noexcept;

        void push_back(const T&) noexcept;
        void pop_back() noexcept;

        void resize(uint64) noexcept;
        void reallocate(uint64) noexcept;
        void clear() noexcept;

    private:
        CTypedBuffer() noexcept;

        static void _native_make(Self*, SCreateInfo&& info) noexcept;

        VkBuffer _handle = {};
        VmaAllocation _allocation = {};
        VkBufferUsageFlags _buf_usage = {};
        VkMemoryPropertyFlags _mem_usage = {};
        void* _data = nullptr;
        uint64 _size = 0;
        uint64 _capacity = 0;
        uint64 _address = 0;

        CRcPtr<CDevice> _device;
    };

    constexpr auto memory_auto = SMemoryProperties();

    template <typename T>
    CTypedBuffer<T>::CTypedBuffer() noexcept = default;

    template <typename T>
    CTypedBuffer<T>::~CTypedBuffer() noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(_device->logger(), "deallocating buffer: {}", (const void*)_handle);
        vmaDestroyBuffer(_device->allocator(), _handle, _allocation);
    }

    template <typename T>
    AM_NODISCARD CRcPtr<CTypedBuffer<T>> CTypedBuffer<T>::make(CRcPtr<CDevice> device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto result = new Self();
        result->_device = std::move(device);
        _native_make(result, std::move(info));
        return CRcPtr<Self>::make(result);
    }

    template <typename T>
    AM_NODISCARD std::vector<CRcPtr<CTypedBuffer<T>>> CTypedBuffer<T>::make(const CRcPtr<CDevice>& device, uint32 count, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        std::vector<CRcPtr<Self>> result;
        result.reserve(count);
        for (uint32 i = 0; i < count; ++i) {
            result.emplace_back(make(device, std::move(info)));
        }
        return result;
    }

    template <typename T>
    void CTypedBuffer<T>::_native_make(Self* self, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        const auto* device = self->_device.get();
        AM_LIKELY_IF(device->feature_support(EDeviceFeature::BufferDeviceAddress)) {
            info.usage |= EBufferUsage::ShaderDeviceAddress;
        }

        uint32 queue_families[3] = {
            device->graphics_queue()->family(),
            device->transfer_queue()->family(),
            device->compute_queue()->family()
        };
        uint32 queue_families_count = 1;
        AM_LIKELY_IF(queue_families[0] != queue_families[1]) {
            queue_families_count++;
        }
        AM_LIKELY_IF(queue_families[0] != queue_families[2] ||
                     queue_families[1] != queue_families[2]) {
            queue_families_count++;
        }

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = info.capacity * sizeof(T);
        buffer_info.usage = prv::as_vulkan(info.usage);
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        AM_UNLIKELY_IF(info.shared && queue_families_count >= 2) {
            buffer_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
            buffer_info.queueFamilyIndexCount = queue_families_count;
            buffer_info.pQueueFamilyIndices = queue_families;
        }

        if (prv::as_underlying(info.usage & (EBufferUsage::VertexBuffer | EBufferUsage::IndexBuffer))) {
            info.memory.required |= EMemoryProperty::DeviceLocal;
        }

        if (prv::as_underlying(info.usage & (EBufferUsage::UniformBuffer | EBufferUsage::StorageBuffer | EBufferUsage::IndirectBuffer))) {
            info.memory.required |= EMemoryProperty::HostVisible | EMemoryProperty::HostCoherent;
            info.memory.preferred |= EMemoryProperty::DeviceLocal;
        }

        if (info.staging) {
            info.memory.required = EMemoryProperty::HostVisible;
        }

        VmaAllocationCreateInfo allocation_info = {};
        if (prv::as_underlying(info.memory.required & EMemoryProperty::HostVisible)) {
            allocation_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
            if (info.staging) {
                allocation_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            } else {
                allocation_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            }
        }
        allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
        allocation_info.requiredFlags = prv::as_vulkan(info.memory.required);
        allocation_info.preferredFlags = prv::as_vulkan(info.memory.preferred);
        allocation_info.memoryTypeBits = {};
        allocation_info.pool = nullptr;
        allocation_info.pUserData = nullptr;
        allocation_info.priority = 1;

        VmaAllocationInfo extra_info;
        AM_VULKAN_CHECK(
            device->logger(),
            vmaCreateBuffer(
                device->allocator(),
                &buffer_info,
                &allocation_info,
                &self->_handle,
                &self->_allocation,
                &extra_info));
        AM_LOG_INFO(device->logger(), "allocating CTypedBuffer<T>({}), size: {} bytes, address: {}",
                    (const void*)self->_handle, buffer_info.size, (const void*)extra_info.pMappedData);
        self->_buf_usage = buffer_info.usage;
        vmaGetMemoryTypeProperties(device->allocator(), extra_info.memoryType, &self->_mem_usage);
        self->_data = extra_info.pMappedData;
        self->_capacity = info.capacity;
        AM_LIKELY_IF(device->feature_support(EDeviceFeature::BufferDeviceAddress)) {
            VkBufferDeviceAddressInfo device_address_info = {};
            device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            device_address_info.buffer = self->_handle;
            self->_address = vkGetBufferDeviceAddress(device->native(), &device_address_info);
        }
    }

    template <typename T>
    AM_NODISCARD VkBuffer CTypedBuffer<T>::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    template <typename T>
    AM_NODISCARD VkBufferUsageFlags CTypedBuffer<T>::buffer_usage() const noexcept {
        AM_PROFILE_SCOPED();
        return _buf_usage;
    }

    template <typename T>
    AM_NODISCARD VkMemoryPropertyFlags CTypedBuffer<T>::memory_usage() const noexcept {
        AM_PROFILE_SCOPED();
        return _mem_usage;
    }

    template <typename T>
    AM_NODISCARD uint64 CTypedBuffer<T>::size() const noexcept {
        AM_PROFILE_SCOPED();
        return _size;
    }

    template <typename T>
    AM_NODISCARD uint64 CTypedBuffer<T>::capacity() const noexcept {
        AM_PROFILE_SCOPED();
        return _capacity;
    }

    template <typename T>
    AM_NODISCARD uint64 CTypedBuffer<T>::address() const noexcept {
        AM_PROFILE_SCOPED();
        return _address;
    }

    template <typename T>
    AM_NODISCARD bool CTypedBuffer<T>::empty() const noexcept {
        AM_PROFILE_SCOPED();
        return _size == 0;
    }

    template <typename T>
    AM_NODISCARD STypedBufferInfo CTypedBuffer<T>::info(uint64 offset) const noexcept {
        AM_PROFILE_SCOPED();
        const auto size = size_bytes();
        return {
            _handle,
            offset,
            size == 0 ? 4 : size
        };
    }

    template <typename T>
    AM_NODISCARD uint64 CTypedBuffer<T>::size_bytes() const noexcept {
        AM_PROFILE_SCOPED();
        return _size * sizeof(T);
    }

    template <typename T>
    AM_NODISCARD void* CTypedBuffer<T>::raw() noexcept {
        AM_PROFILE_SCOPED();
        return _data;
    }

    template <typename T>
    AM_NODISCARD const void* CTypedBuffer<T>::raw() const noexcept {
        AM_PROFILE_SCOPED();
        return _data;
    }

    template <typename T>
    AM_NODISCARD T* CTypedBuffer<T>::data() noexcept {
        AM_PROFILE_SCOPED();
        return static_cast<T*>(_data);
    }

    template <typename T>
    AM_NODISCARD const T* CTypedBuffer<T>::data() const noexcept {
        AM_PROFILE_SCOPED();
        return static_cast<const T*>(_data);
    }

    template <typename T>
    AM_NODISCARD T& CTypedBuffer<T>::operator [](uint64 index) noexcept {
        AM_PROFILE_SCOPED();
        return *std::launder(data())[index];
    }

    template <typename T>
    AM_NODISCARD const T& CTypedBuffer<T>::operator [](uint64 index) const noexcept {
        AM_PROFILE_SCOPED();
        return *std::launder(data())[index];
    }

    template <typename T>
    void CTypedBuffer<T>::insert(const T& value) noexcept {
        AM_PROFILE_SCOPED();
        insert(0, value);
    }

    template <typename T>
    void CTypedBuffer<T>::insert(uint64 where, const T& value) noexcept {
        AM_PROFILE_SCOPED();
        AM_UNLIKELY_IF(where > _capacity) {
            reallocate(std::max(_capacity * 2, where));
        }
        std::memcpy(data() + where, &value, sizeof(T));
        _size = std::max(where + 1, _size);
    }

    template <typename T>
    void CTypedBuffer<T>::insert(const void* ptr, uint64 bytes) noexcept {
        AM_PROFILE_SCOPED();
        const auto size = bytes / sizeof(T);
        AM_UNLIKELY_IF(size > _capacity) {
            reallocate(std::max(_capacity * 2, size));
        }
        std::memcpy(_data, ptr, bytes);
        _size = size;
    }

    template <typename T>
    void CTypedBuffer<T>::insert(const std::vector<T>& values) noexcept {
        AM_PROFILE_SCOPED();
        insert(0, values);
    }

    template <typename T>
    void CTypedBuffer<T>::insert(uint64 where, const std::vector<T>& values) noexcept {
        AM_PROFILE_SCOPED();
        const auto size = values.size() + where;
        AM_UNLIKELY_IF(size > _capacity) {
            reallocate(std::max(_capacity * 2, size));
        }
        std::memcpy(data() + where, values.data(), am::size_bytes(values));
        _size = std::max(size, _size);
    }

    template <typename T>
    void CTypedBuffer<T>::push_back(const T& value) noexcept {
        AM_PROFILE_SCOPED();
        AM_UNLIKELY_IF(_size == _capacity) {
            reallocate(std::max(_capacity * 2, _size));
        }
        std::memcpy(data() + _size, &value, sizeof(T));
        _size++;
    }

    template <typename T>
    void CTypedBuffer<T>::pop_back() noexcept {
        AM_PROFILE_SCOPED();
        AM_ASSERT(_size != 0, "pop_back() failure: empty");
        --_size;
    }

    template <typename T>
    void CTypedBuffer<T>::resize(uint64 size) noexcept {
        AM_PROFILE_SCOPED();
        AM_UNLIKELY_IF(size >= _capacity) {
            reallocate(std::max(_capacity * 2, size));
        }
        _size = size;
    }

    template <typename T>
    void CTypedBuffer<T>::reallocate(uint64 capacity) noexcept {
        AM_PROFILE_SCOPED();
        AM_LIKELY_IF(_capacity >= capacity) {
            return;
        }
        AM_LOG_WARN(_device->logger(), "buffer reallocation requested: {} -> {}", _capacity, capacity);
        const void* old_data = _data;
        auto old_buffer = _handle;
        auto old_allocation = _allocation;
        _native_make(this, {
            .usage = static_cast<EBufferUsage>(buffer_usage()),
            .memory = {
                .required = static_cast<EMemoryProperty>(memory_usage()),
                .preferred = {}
            },
            .capacity = capacity,
            .staging = false,
        });
        _capacity = capacity;
        _size = std::min(_size, _capacity);
        AM_LIKELY_IF(old_data) {
            std::memcpy(_data, old_data, Self::size_bytes());
        }
        vmaDestroyBuffer(_device->allocator(), old_buffer, old_allocation);
    }

    template <typename T>
    void CTypedBuffer<T>::clear() noexcept {
        AM_PROFILE_SCOPED();
        _size = 0;
    }
} // namespace am
