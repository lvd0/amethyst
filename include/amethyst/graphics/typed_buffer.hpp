#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/queue.hpp>

#include <amethyst/meta/constants.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/types.hpp>

#if _WIN64
    #include <vulkan/vulkan_win32.h>
#endif

#include <vulkan/vulkan.h>
#include <volk.h>

#include <vk_mem_alloc.h>

#include <cstring>
#include <vector>

namespace am {
    struct SBufferInfo {
        VkBuffer handle = {};
        uint64 offset = 0;
        uint64 size = 0;
        uint64 address = 0;
    };

    struct SMemoryProperties {
        EMemoryProperty required = {};
        EMemoryProperty preferred = {};

        AM_NODISCARD constexpr bool operator ==(const SMemoryProperties& other) const noexcept {
            return prv::as_underlying(required) == prv::as_underlying(other.required) &&
                   prv::as_underlying(preferred) == prv::as_underlying(other.preferred);
        }
    };

    template <typename T>
    class AM_MODULE CTypedBuffer : public IRefCounted {
    public:
        using Self = CTypedBuffer;
        using BoxedType = T;
        struct SCreateInfo {
            EBufferUsage usage = {};
            SMemoryProperties memory = {};
            uint64 capacity = 0;
            bool staging = false;
            bool shared = false;
            bool external = false;
        };

        ~CTypedBuffer() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, SCreateInfo&&) noexcept;
        AM_NODISCARD static std::vector<CRcPtr<Self>> make(const CRcPtr<CDevice>&, uint32, SCreateInfo&&) noexcept;

        AM_NODISCARD VkBuffer native() const noexcept;
        AM_NODISCARD VkDeviceMemory memory() const noexcept;
        AM_NODISCARD VkBufferUsageFlags buffer_usage() const noexcept;
        AM_NODISCARD VkMemoryPropertyFlags memory_usage() const noexcept;
        AM_NODISCARD uint64 alignment() const noexcept;
        AM_NODISCARD uint64 size() const noexcept;
        AM_NODISCARD uint64 capacity() const noexcept;
        AM_NODISCARD uint64 address() const noexcept;
        AM_NODISCARD void* external_handle() const noexcept;

        AM_NODISCARD bool empty() const noexcept;
        AM_NODISCARD SBufferInfo info(uint64 = 0) const noexcept;
        AM_NODISCARD uint64 alloc_size() const noexcept;
        AM_NODISCARD uint64 alloc_offset() const noexcept;
        AM_NODISCARD uint64 size_bytes() const noexcept;

        AM_NODISCARD void* raw() noexcept;
        AM_NODISCARD const void* raw() const noexcept;

        AM_NODISCARD T* data() noexcept;
        AM_NODISCARD const T* data() const noexcept;

        AM_NODISCARD T& operator [](uint64) noexcept;
        AM_NODISCARD const T& operator [](uint64) const noexcept;

        void insert(const T&) noexcept;
        void insert(uint64, const T&) noexcept;
        void insert(const void*, uint64, uint64 = 0) noexcept;
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
        VkDeviceMemory _memory = {};
        VmaAllocation _allocation = {};
        VkBufferUsageFlags _buf_usage = {};
        VkMemoryPropertyFlags _mem_usage = {};
        void* _data = nullptr;
        uint64 _alignment = 0;
        uint64 _alloc_size = 0;
        uint64 _alloc_offset = 0;
        uint64 _size = 0;
        uint64 _capacity = 0;
        uint64 _address = 0;
        void* _external_handle = nullptr;
        bool _shared = false;
        bool _external = false;

        CRcPtr<CDevice> _device;
    };

    constexpr auto memory_auto = SMemoryProperties{ (EMemoryProperty)0, (EMemoryProperty)0 };

    template <typename T>
    CTypedBuffer<T>::CTypedBuffer() noexcept = default;

    template <typename T>
    CTypedBuffer<T>::~CTypedBuffer() noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(_device->logger(), "deallocating buffer: {}", (const void*)_handle);
        AM_UNLIKELY_IF(_external_handle) {
#if _WIN64
            CloseHandle(_external_handle);
#endif
        }
        if (_external) {
            vkFreeMemory(_device->native(), _memory, nullptr);
            vkDestroyBuffer(_device->native(), _handle, nullptr);
        } else {
            vmaDestroyBuffer(_device->allocator(), _handle, _allocation);
        }
    }

    template <typename T>
    AM_NODISCARD CRcPtr<CTypedBuffer<T>> CTypedBuffer<T>::make(CRcPtr<CDevice> device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
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
        auto* device = self->_device.get();
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
        AM_LIKELY_IF(queue_families[0] != queue_families[2] &&
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

        VkExternalMemoryBufferCreateInfo external_memory = {};
        AM_UNLIKELY_IF(info.external) {
            external_memory.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
            external_memory.handleTypes = external_memory_handle_type;
            buffer_info.pNext = &external_memory;
        }

        AM_LIKELY_IF(info.memory == memory_auto) {
            if (prv::as_underlying(info.usage & (EBufferUsage::VertexBuffer | EBufferUsage::IndexBuffer))) {
                info.memory.required = EMemoryProperty::DeviceLocal;
            }

            if (prv::as_underlying(info.usage & (EBufferUsage::UniformBuffer | EBufferUsage::StorageBuffer | EBufferUsage::IndirectBuffer))) {
                info.memory.required = EMemoryProperty::HostVisible | EMemoryProperty::HostCoherent;
                info.memory.preferred = EMemoryProperty::DeviceLocal;
            }

            if (info.staging) {
                info.memory.required = EMemoryProperty::HostVisible | EMemoryProperty::HostCoherent;
            }
        }

        VmaAllocationInfo extra_info = {};
        VkMemoryRequirements memory_requirements = {};
        if (info.external) {
            AM_VULKAN_CHECK(device->logger(), vkCreateBuffer(device->native(), &buffer_info, nullptr, &self->_handle));
            vkGetBufferMemoryRequirements(device->native(), self->_handle, &memory_requirements);
            memory_requirements.size = align_size(memory_requirements.size, memory_requirements.alignment);
            const auto memory_index = device->memory_type_index(memory_requirements.memoryTypeBits, info.memory.required);
            VkExportMemoryAllocateInfo export_allocate_info = {};
#if _WIN64
            VkExportMemoryWin32HandleInfoKHR win32_ext_mem_handle_info = {};
            export_allocate_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
            export_allocate_info.handleTypes = external_memory_handle_type;
            CWindowsSecurityAttributes security_attributes;
            win32_ext_mem_handle_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
            win32_ext_mem_handle_info.pAttributes = security_attributes.get();
            win32_ext_mem_handle_info.dwAccess = DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE;
            win32_ext_mem_handle_info.name = nullptr;
            export_allocate_info.pNext = &win32_ext_mem_handle_info;
#endif

            VkMemoryAllocateFlagsInfo memory_allocate_flags = {};
            memory_allocate_flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            memory_allocate_flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
            VkMemoryAllocateInfo memory_allocate_info = {};
            memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memory_allocate_info.pNext = &export_allocate_info;
            memory_allocate_info.allocationSize = memory_requirements.size;
            memory_allocate_info.memoryTypeIndex = memory_index;
            if (buffer_info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
                memory_allocate_info.pNext = &memory_allocate_flags;
                memory_allocate_flags.pNext = &export_allocate_info;
            }
            AM_VULKAN_CHECK(device->logger(), vkAllocateMemory(device->native(), &memory_allocate_info, nullptr, &self->_memory));
            AM_VULKAN_CHECK(device->logger(), vkBindBufferMemory(device->native(), self->_handle, self->_memory, 0));
            self->_alloc_size = memory_requirements.size;
            self->_alloc_offset = 0;
            self->_capacity = memory_requirements.size;
            self->_data = nullptr;
            self->_mem_usage = prv::as_vulkan(info.memory.required);
        } else {
            VmaAllocationCreateInfo allocation_info = {};
            AM_LIKELY_IF(prv::as_underlying(info.memory.required & EMemoryProperty::HostVisible)) {
                info.memory.required |= EMemoryProperty::HostCached;
                allocation_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            }
            allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
            allocation_info.requiredFlags = prv::as_vulkan(info.memory.required);
            allocation_info.preferredFlags = prv::as_vulkan(info.memory.preferred);
            allocation_info.memoryTypeBits = {};
            AM_VULKAN_CHECK(
                device->logger(),
                vmaCreateBuffer(
                    device->allocator(),
                    &buffer_info,
                    &allocation_info,
                    &self->_handle,
                    &self->_allocation,
                    &extra_info));
            self->_memory = extra_info.deviceMemory;
            self->_alloc_size = extra_info.size;
            self->_alloc_offset = extra_info.offset;
            self->_capacity = info.capacity;
            self->_data = extra_info.pMappedData;
            vmaGetMemoryTypeProperties(device->allocator(), extra_info.memoryType, &self->_mem_usage);
        }
        AM_LOG_INFO(device->logger(), "allocating CTypedBuffer<T>({}), size: {} bytes, address: {}",
                    (const void*)self->_handle, buffer_info.size, (const void*)extra_info.pMappedData);
        vkGetBufferMemoryRequirements(device->native(), self->_handle, &memory_requirements);
        self->_buf_usage = buffer_info.usage;
        self->_shared = info.shared;
        self->_external = info.external;
        self->_alignment = memory_requirements.alignment;
        if (info.external) {
#if _WIN64
            VkMemoryGetWin32HandleInfoKHR memory_handle_info = {};
            memory_handle_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
            memory_handle_info.memory = self->_memory;
            memory_handle_info.handleType = external_memory_handle_type;
            AM_VULKAN_CHECK(device->logger(), vkGetMemoryWin32HandleKHR(device->native(), &memory_handle_info, &self->_external_handle));
#else
            int handle_fd = -1;
            VkMemoryGetFdInfoKHR memory_handle_info = {};
            memory_handle_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
            memory_handle_info.memory = self->_memory;
            memory_handle_info.handleType = external_memory_handle_type;
            AM_VULKAN_CHECK(device->logger(), vkGetMemoryFdKHR(device->native(), &memory_handle_info, &handle_fd));
            self->_external_handle = (void*)(uint64)handle_fd;
#endif
        }

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
    AM_NODISCARD VkDeviceMemory CTypedBuffer<T>::memory() const noexcept {
        AM_PROFILE_SCOPED();
        return _memory;
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
    AM_NODISCARD uint64 CTypedBuffer<T>::alignment() const noexcept {
        AM_PROFILE_SCOPED();
        return _alignment;
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
    AM_NODISCARD void* CTypedBuffer<T>::external_handle() const noexcept {
        AM_PROFILE_SCOPED();
        return _external_handle;
    }

    template <typename T>
    AM_NODISCARD bool CTypedBuffer<T>::empty() const noexcept {
        AM_PROFILE_SCOPED();
        return _size == 0;
    }

    template <typename T>
    AM_NODISCARD SBufferInfo CTypedBuffer<T>::info(uint64 offset) const noexcept {
        AM_PROFILE_SCOPED();
        const auto size = size_bytes();
        return {
            _handle,
            offset * sizeof(T),
            size == 0 ? 1 : size
        };
    }

    template <typename T>
    AM_NODISCARD uint64 CTypedBuffer<T>::alloc_size() const noexcept {
        AM_PROFILE_SCOPED();
        return _alloc_size;
    }

    template <typename T>
    AM_NODISCARD uint64 CTypedBuffer<T>::alloc_offset() const noexcept {
        AM_PROFILE_SCOPED();
        return _alloc_offset;
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
        return data()[index];
    }

    template <typename T>
    AM_NODISCARD const T& CTypedBuffer<T>::operator [](uint64 index) const noexcept {
        AM_PROFILE_SCOPED();
        return data()[index];
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
    void CTypedBuffer<T>::insert(const void* ptr, uint64 bytes, uint64 offset) noexcept {
        AM_PROFILE_SCOPED();
        const auto size = bytes / sizeof(T);
        AM_UNLIKELY_IF(size > _capacity) {
            reallocate(std::max(_capacity * 2, size));
        }
        std::memcpy(data() + offset, ptr, bytes);
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
            static_cast<EBufferUsage>(buffer_usage()),
            { static_cast<EMemoryProperty>(memory_usage()) },
            capacity,
            false,
            _shared
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
