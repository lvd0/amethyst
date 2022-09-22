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

#include <vulkan/vulkan.h>
#include <volk.h>

#include <vk_mem_alloc.h>

#if defined(AM_ENABLE_AFTERMATH)
    #include <GFSDK_Aftermath.h>
    #include <GFSDK_Aftermath_Defines.h>
    #include <GFSDK_Aftermath_GpuCrashDump.h>
#endif

#include <unordered_map>
#include <queue>

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

        AM_NODISCARD const VkPhysicalDeviceLimits& limits() const noexcept;
        AM_NODISCARD bool feature_support(EDeviceFeature) const noexcept;

        AM_NODISCARD CBufferSuballocator* vertex_buffer_allocator() noexcept;
        AM_NODISCARD CBufferSuballocator* index_buffer_allocator() noexcept;

        AM_NODISCARD uint32 acquire_image(CSwapchain*, const CSemaphore*) const noexcept;
        AM_NODISCARD STextureInfo sample(const CAsyncTexture*, SSamplerInfo) noexcept;
        AM_NODISCARD STextureInfo sample(const CImage*, SSamplerInfo) noexcept;

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

        VkDevice _handle = {};
        VkPhysicalDevice _gpu = {};
        VmaAllocator _allocator = {};
        VkPhysicalDeviceVulkan11Features _features_11 = {};
        VkPhysicalDeviceVulkan12Features _features_12 = {};
        VkPhysicalDeviceVulkan13Features _features_13 = {};
        VkPhysicalDeviceFeatures2 _features = {};
        VkPhysicalDeviceProperties _properties = {};
        struct {
            bool debug_names = false;
        } _features_custom;

        CQueue* _graphics = nullptr;
        CQueue* _transfer = nullptr;
        CQueue* _compute = nullptr;

        std::unique_ptr<CBufferSuballocator> _vertex_buffer_suballocator;
        std::unique_ptr<CBufferSuballocator> _index_buffer_suballocator;

        DescriptorSetLayoutCache _set_layout_cache;
        SamplerCache _sampler_cache;

        std::queue<SCleanupPayload> _to_delete;

#if defined(AM_ENABLE_AFTERMATH)
        std::unique_ptr<CGPUCrashTrackerNV> _aftermath_context;
#endif

        CRcPtr<CContext> _context;
        std::shared_ptr<spdlog::logger> _logger;
    };
} // namespace am
