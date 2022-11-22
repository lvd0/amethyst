#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/meta/debug_marker.hpp>
#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#if defined(_MSC_VER)
    #pragma warning(push, 0)
#endif

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

#include <volk.h>
#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>

#if _WIN64
    #include <VersionHelpers.h>
    #include <dxgi1_2.h>
    #include <aclapi.h>
#endif

#if defined(AM_ENABLE_AFTERMATH)
    #include <GFSDK_Aftermath.h>
    #include <GFSDK_Aftermath_Defines.h>
    #include <GFSDK_Aftermath_GpuCrashDump.h>
#endif

#include <unordered_map>
#include <deque>

namespace am {
    enum class EDeviceExtension {
        Swapchain,
        Raytracing,
        ExternalMemory,
        BufferDeviceAddress,
        FragmentShaderBarycentric
    };

    enum class EDeviceFeature {
        DebugNames,
        BufferDeviceAddress
    };

    enum class EVirtualAllocatorKind : uint32 {
        VertexBuffer,
        IndexBuffer,
        StagingBuffer,
        Count
    };

#if defined(AM_ENABLE_AFTERMATH)
    class CGPUCrashTrackerNV {
    public:
        CGPUCrashTrackerNV() noexcept;
        ~CGPUCrashTrackerNV() noexcept;

        void on_crash_dump(const void*, uint32) noexcept;
        void on_shader_debug_info(const void*, uint32) noexcept;
        void on_description(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription) noexcept;

    private:
        std::mutex lock;
    };
#endif

    struct SHeapBudget {
        uint32 block_count;
        uint32 allocation_count;
        uint64 block_bytes;
        uint64 allocation_bytes;
    };

#if _WIN64
    class CWindowsSecurityAttributes {
    public:
        CWindowsSecurityAttributes();
        ~CWindowsSecurityAttributes();

        const SECURITY_ATTRIBUTES* get() const noexcept;

    private:
        SECURITY_ATTRIBUTES security_attributes = {};
        PSECURITY_DESCRIPTOR security_descriptor = {};
    };
#endif

    class AM_MODULE CDevice : public IRefCounted {
    public:
        using Self = CDevice;
        using DescriptorSetLayoutCache = std::unordered_map<uint64, VkDescriptorSetLayout>;
        using SamplerCache = std::unordered_map<uint64, VkSampler>;
        struct SCreateInfo {
            std::vector<EDeviceExtension> extensions;
        };

        ~CDevice() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CContext>, SCreateInfo&&) noexcept;

        AM_NODISCARD VkDevice native() const noexcept;
        AM_NODISCARD VkPhysicalDevice gpu() const noexcept;
        AM_NODISCARD VmaAllocator allocator() const noexcept;

        AM_NODISCARD CQueue* graphics_queue() const noexcept;
        AM_NODISCARD CQueue* transfer_queue() const noexcept;
        AM_NODISCARD CQueue* compute_queue() const noexcept;

        AM_NODISCARD CContext* context() noexcept;
        AM_NODISCARD const CContext* context() const noexcept;

        AM_NODISCARD spdlog::logger* logger() const noexcept;

        AM_NODISCARD const VkPhysicalDeviceProperties& properties() const noexcept;
        AM_NODISCARD const VkPhysicalDeviceLimits& limits() const noexcept;
        AM_NODISCARD const uint8 (&uuid() const noexcept)[VK_UUID_SIZE];
        AM_NODISCARD bool feature_support(EDeviceFeature) const noexcept;
        AM_NODISCARD std::vector<SHeapBudget> heap_budgets() const noexcept;

        AM_NODISCARD CVirtualAllocator* virtual_allocator(EVirtualAllocatorKind) noexcept;
        AM_NODISCARD uint32 memory_type_index(uint32, EMemoryProperty) noexcept;
        AM_NODISCARD const VkExportMemoryAllocateInfo* external_memory_attributes() noexcept;

        AM_NODISCARD uint32 acquire_image(CSwapchain*, const CSemaphore*) const noexcept;
        AM_NODISCARD STextureInfo sample(const CAsyncTexture*, SSamplerInfo, bool = false) noexcept;
        AM_NODISCARD STextureInfo sample(const CImage*, SSamplerInfo, bool = false) noexcept;
        AM_NODISCARD STextureInfo sample(const CImageView*, SSamplerInfo, bool = false) noexcept;

        AM_NODISCARD VkDescriptorSetLayout acquire_cached_item(const std::vector<SDescriptorBinding>&) noexcept;
        void set_cached_item(const std::vector<SDescriptorBinding>&, VkDescriptorSetLayout) noexcept;

        AM_NODISCARD VkSampler acquire_cached_item(const SSamplerInfo&) noexcept;
        void set_cached_item(const SSamplerInfo&, VkSampler) noexcept;

        void cleanup_after(uint32, std::function<void(const Self*)>&&) noexcept;
        void update_cleanup() noexcept;

        void wait_idle() const noexcept;

    private:
        struct SCleanupPayload {
            const uint32 _frames = 0;
            uint32 _elapsed = 0;
            std::function<void(const Self*)> _func;
        };

        CDevice() noexcept;

        AM_NODISCARD VkSampler _make_sampler(SSamplerInfo, bool) noexcept;

        VkDevice _handle = {};
        VkPhysicalDevice _gpu = {};
        VmaAllocator _allocator = {};
        VkPhysicalDeviceVulkan11Features _features_11 = {};
        VkPhysicalDeviceVulkan12Features _features_12 = {};
        VkPhysicalDeviceVulkan13Features _features_13 = {};
        VkPhysicalDeviceFeatures2 _features = {};
        VkPhysicalDeviceProperties2 _properties = {};
        VkPhysicalDeviceIDProperties _gpu_id = {};
        VkPhysicalDeviceMemoryProperties _memory_props = {};
        struct {
            bool debug_names = false;
        } _features_custom;

        CQueue* _graphics = nullptr;
        CQueue* _transfer = nullptr;
        CQueue* _compute = nullptr;

        std::vector<std::unique_ptr<CVirtualAllocator>> _virtual_allocators;

        DescriptorSetLayoutCache _set_layout_cache;
        SamplerCache _sampler_cache;

        std::deque<SCleanupPayload> _to_delete;

#if defined(AM_ENABLE_AFTERMATH)
        std::unique_ptr<CGPUCrashTrackerNV> _aftermath_context;
#endif

        CRcPtr<CContext> _context;
        std::shared_ptr<spdlog::logger> _logger;
    };
} // namespace am
