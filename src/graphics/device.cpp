#include <amethyst/graphics/virtual_allocator.hpp>
#include <amethyst/graphics/async_texture.hpp>
#include <amethyst/graphics/semaphore.hpp>
#include <amethyst/graphics/swapchain.hpp>
#include <amethyst/graphics/pipeline.hpp>
#include <amethyst/graphics/context.hpp>
#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/queue.hpp>

#include <amethyst/meta/constants.hpp>
#include <amethyst/meta/hash.hpp>

#if _WIN64
    #include <vulkan/vulkan_win32.h>
#endif

#include <optional>
#include <fstream>
#include <chrono>
#include <cmath>

#if defined(AM_ENABLE_AFTERMATH)
    AM_NORETURN void aftermath_finalize_crash() noexcept {
        auto status = GFSDK_Aftermath_CrashDump_Status_Unknown;
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            AM_ASSERT(GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GetCrashDumpStatus(&status)), "Aftermath: failure");
        } while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed &&
                 status != GFSDK_Aftermath_CrashDump_Status_Finished);
        AM_ASSERT(false, "Aftermath: finalizing crash");
    }
#endif

namespace am {
    template <typename T, typename U>
    static inline void insert_chain(T* self, U* object) noexcept {
        auto* old = self->pNext;
        self->pNext = object;
        object->pNext = old;
    }

#if defined(AM_ENABLE_AFTERMATH)
    CGPUCrashTrackerNV::CGPUCrashTrackerNV() noexcept = default;

    CGPUCrashTrackerNV::~CGPUCrashTrackerNV() noexcept = default;

    void CGPUCrashTrackerNV::on_crash_dump(const void* dump, uint32 size) noexcept {
        static uint64 index = 0;
        std::lock_guard guard(lock);
        std::ofstream("aftermath" + std::to_string(index++) + ".nv-gpudmp", std::ios::binary).write((const char*)dump, size);
    }

    void CGPUCrashTrackerNV::on_shader_debug_info(const void* dump, uint32 size) noexcept {
        static uint64 index = 0;
        std::lock_guard guard(lock);
        std::ofstream("shader_debug" + std::to_string(index++) + ".nvdbg", std::ios::binary).write((const char*)dump, size);
    }

    void CGPUCrashTrackerNV::on_description(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_desc) noexcept {
        add_desc(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "Amethyst Engine");
    }
#endif

#if _WIN64
    CWindowsSecurityAttributes::CWindowsSecurityAttributes() {
        security_descriptor = (PSECURITY_DESCRIPTOR)std::calloc(1, SECURITY_DESCRIPTOR_MIN_LENGTH + 2 * sizeof(void**));
        InitializeSecurityDescriptor(security_descriptor, SECURITY_DESCRIPTOR_REVISION);

        PSID* pp_sid = (PSID*)((PBYTE)security_descriptor + SECURITY_DESCRIPTOR_MIN_LENGTH);
        PACL* pp_acl = (PACL*)((PBYTE)pp_sid + sizeof(PSID*));

        SID_IDENTIFIER_AUTHORITY sid_id_auth = SECURITY_WORLD_SID_AUTHORITY;
        AllocateAndInitializeSid(&sid_id_auth, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, pp_sid);

        EXPLICIT_ACCESS explicit_access;
        ZeroMemory(&explicit_access, sizeof(EXPLICIT_ACCESS));
        explicit_access.grfAccessPermissions = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
        explicit_access.grfAccessMode = SET_ACCESS;
        explicit_access.grfInheritance = INHERIT_ONLY;
        explicit_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        explicit_access.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        explicit_access.Trustee.ptstrName = (LPTSTR)*pp_sid;
        SetEntriesInAcl(1, &explicit_access, nullptr, pp_acl);

        SetSecurityDescriptorDacl(security_descriptor, true, *pp_acl, false);
        security_attributes.nLength = sizeof(security_attributes);
        security_attributes.lpSecurityDescriptor = security_descriptor;
        security_attributes.bInheritHandle = true;
    }

    CWindowsSecurityAttributes::~CWindowsSecurityAttributes() {
        PSID* pp_sid = (PSID*)((PBYTE)security_descriptor + SECURITY_DESCRIPTOR_MIN_LENGTH);
        PACL* pp_acl = (PACL*)((PBYTE)pp_sid + sizeof(PSID*));
        FreeSid(*pp_sid);
        LocalFree(*pp_acl);
        std::free(security_descriptor);
    }

    const SECURITY_ATTRIBUTES* CWindowsSecurityAttributes::get() const noexcept {
        return &security_attributes;
    }
#endif

    CDevice::CDevice() noexcept = default;

    CDevice::~CDevice() noexcept {
        AM_PROFILE_SCOPED();
#if defined(AM_ENABLE_AFTERMATH)
        GFSDK_Aftermath_DisableGpuCrashDumps();
#endif
        while (!_to_delete.empty()) {
            _to_delete.front()._func(this);
            _to_delete.pop_front();
        }
        for (const auto& [_, layout] : _set_layout_cache) {
            vkDestroyDescriptorSetLayout(_handle, layout, nullptr);
        }
        for (const auto& [_, sampler] : _sampler_cache) {
            vkDestroySampler(_handle, sampler, nullptr);
        }
        AM_LOG_INFO(_logger, "terminating allocator");
        _virtual_allocators.clear();
        vmaDestroyAllocator(_allocator);
        delete _graphics;
        AM_UNLIKELY_IF(_graphics != _transfer) {
            delete _transfer;
        }
        AM_UNLIKELY_IF(_transfer != _compute) {
            delete _compute;
        }
        AM_LOG_INFO(_logger, "terminating device: {}", _properties.properties.deviceName);
        vkDestroyDevice(_handle, nullptr);
    }

    AM_NODISCARD CRcPtr<CDevice> CDevice::make(CRcPtr<CContext> context, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        auto logger = spdlog::stdout_color_mt("device");
        AM_LOG_INFO(logger, "initializing device");
        VkPhysicalDeviceVulkan11Features features_11 = {};
        features_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        VkPhysicalDeviceVulkan12Features features_12 = {};
        features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features_12.pNext = &features_11;
        VkPhysicalDeviceVulkan13Features features_13 = {};
        features_13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features_13.pNext = &features_12;
        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        uint32 api_version;
        AM_VULKAN_CHECK(logger, vkEnumerateInstanceVersion(&api_version));
        { // Physical Device selection
            uint32 count;
            vkEnumeratePhysicalDevices(context->native(), &count, nullptr);
            std::vector<VkPhysicalDevice> devices(count);
            vkEnumeratePhysicalDevices(context->native(), &count, devices.data());
            for (const auto gpu : devices) {
                result->_gpu_id.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
                VkPhysicalDeviceProperties2 properties2 = {};
                properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                properties2.pNext = &result->_gpu_id;
                vkGetPhysicalDeviceProperties2(gpu, &properties2);
                AM_LOG_INFO(logger, "- found device: {}", properties2.properties.deviceName);
                const auto device_criteria =
                    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU |
                    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU |
                    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU;
                AM_LIKELY_IF(properties2.properties.deviceType & device_criteria) {
                    AM_LOG_INFO(logger, "- chosen device: {}", properties2.properties.deviceName);
                    const auto driver_major = VK_VERSION_MAJOR(properties2.properties.driverVersion);
                    const auto driver_minor = VK_VERSION_MINOR(properties2.properties.driverVersion);
                    const auto driver_patch = VK_VERSION_PATCH(properties2.properties.driverVersion);
                    const auto vulkan_major = VK_API_VERSION_MAJOR(properties2.properties.apiVersion);
                    const auto vulkan_minor = VK_API_VERSION_MINOR(properties2.properties.apiVersion);
                    const auto vulkan_patch = VK_API_VERSION_PATCH(properties2.properties.apiVersion);
                    AM_LOG_INFO(logger, "- driver version: {}.{}.{}", driver_major, driver_minor, driver_patch);
                    AM_LOG_INFO(logger, "- vulkan version: {}.{}.{}", vulkan_major, vulkan_minor, vulkan_patch);
                    if (api_version < VK_API_VERSION_1_3) {
                        features2.pNext = &features_12;
                    } else {
                        features2.pNext = &features_13;
                    }
                    vkGetPhysicalDeviceFeatures2(gpu, &features2);
                    vkGetPhysicalDeviceMemoryProperties(gpu, &result->_memory_props);
                    result->_properties = properties2;
                    result->_features_11 = features_11;
                    result->_features_12 = features_12;
                    result->_features_13 = features_13;
                    result->_features = features2;
                    result->_gpu = gpu;
                    break;
                }
            }
            AM_ASSERT(result->_gpu, "no suitable GPU found in the system");
        }
        bool add_ext_memory = false;
        { // Logical Device selection
            AM_LOG_INFO(logger, "initializing device queues");
            uint32 families_count;
            vkGetPhysicalDeviceQueueFamilyProperties(result->_gpu, &families_count, nullptr);
            std::vector<VkQueueFamilyProperties> queue_families(families_count);
            vkGetPhysicalDeviceQueueFamilyProperties(result->_gpu, &families_count, queue_families.data());

            std::vector<uint32> queue_sizes(families_count);
            std::vector<std::vector<float32>> queue_priorities(families_count);
            const auto fetch_family = [&](VkQueueFlags required, VkQueueFlags ignore, float32 priority) noexcept -> std::optional<SQueueFamily> {
                for (uint32 family = 0; family < families_count; family++) {
                    AM_UNLIKELY_IF((queue_families[family].queueFlags & ignore) != 0) {
                        continue;
                    }

                    const auto remaining = queue_families[family].queueCount- queue_sizes[family];
                    AM_LIKELY_IF(remaining > 0 && (queue_families[family].queueFlags & required) == required) {
                        queue_priorities[family].emplace_back(priority);
                        return SQueueFamily{
                            family, queue_sizes[family]++
                        };
                    }
                }
                return std::nullopt;
            };
            SQueueFamily graphics_family = {};
            SQueueFamily transfer_family = {};
            SQueueFamily compute_family = {};
            if (auto queue = fetch_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0, 0.5f)) {
                graphics_family = *queue;
            } else {
                AM_ASSERT(false, "no suitable graphics queue");
            }

            if (auto queue = fetch_family(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT, 1.0f)) {
                // Prefer a dedicated compute queue which doesn't support graphics.
                compute_family = *queue;
            } else if (auto fallback = fetch_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0, 1.0f)) {
                // Fallback to another graphics queue but we can do async graphics that way.
                compute_family = *fallback;
            } else {
                // Finally, fallback to graphics queue
                compute_family = graphics_family;
            }

            if (auto queue = fetch_family(VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0.5f)) {
                // Find a queue which only supports transfer.
                transfer_family = *queue;
            } else if (auto fallback = fetch_family(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT, 0.5f)) {
                // Fallback to a dedicated compute queue.
                transfer_family = *fallback;
            } else {
                // Finally, fallback to same queue as compute.
                transfer_family = compute_family;
            }
            std::vector<VkDeviceQueueCreateInfo> queue_infos;
            for (uint32 family = 0; family < families_count; family++) {
                AM_UNLIKELY_IF(queue_sizes[family] == 0) {
                    continue;
                }

                VkDeviceQueueCreateInfo queue_info;
                queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queue_info.pNext = nullptr;
                queue_info.flags = {};
                queue_info.queueFamilyIndex = family;
                queue_info.queueCount = queue_sizes[family];
                queue_info.pQueuePriorities = queue_priorities[family].data();
                queue_infos.emplace_back(queue_info);
            }
            AM_LOG_INFO(logger, "enumerating device extensions");
            uint32 ext_count;
            AM_VULKAN_CHECK(logger, vkEnumerateDeviceExtensionProperties(result->_gpu, nullptr, &ext_count, nullptr));
            std::vector<VkExtensionProperties> extensions(ext_count);
            AM_VULKAN_CHECK(logger, vkEnumerateDeviceExtensionProperties(result->_gpu, nullptr, &ext_count, extensions.data()));
            const auto has_extension = [&extensions](const char* name) noexcept {
                for (const auto& ext : extensions) {
                    if (std::strcmp(ext.extensionName, name) == 0) {
                        return true;
                    }
                }
                return false;
            };
            std::vector<const char*> enabled_extensions = {};
            enabled_extensions.reserve(info.extensions.size());
            VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracing_features = {};
            VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bda_features = {};
            VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV barycentric_features = {};
            for (const auto each : info.extensions) {
                switch (each) {
                    case EDeviceExtension::Swapchain: {
                        enabled_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
                    } break;

                    case EDeviceExtension::Raytracing: {
                        raytracing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
                        raytracing_features.rayTracingPipeline = true;
                        raytracing_features.rayTracingPipelineTraceRaysIndirect = true;
                        raytracing_features.rayTraversalPrimitiveCulling = true;
                        insert_chain(&features2, &raytracing_features);
                        enabled_extensions.emplace_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
                    } break;

                    case EDeviceExtension::ExternalMemory: {
                        if (api_version < VK_API_VERSION_1_2) {
                            enabled_extensions.emplace_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
                            enabled_extensions.emplace_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
                        }
#if _WIN64
                        enabled_extensions.emplace_back(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
#else
                        enabled_extensions.emplace_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
#endif
                        add_ext_memory = true;
                    } break;

                    case EDeviceExtension::BufferDeviceAddress: {
                        bda_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
                        bda_features.bufferDeviceAddress = true;
                        bda_features.bufferDeviceAddressCaptureReplay = true;
                        insert_chain(&features2, &bda_features);
                        enabled_extensions.emplace_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
                    } break;

                    case EDeviceExtension::FragmentShaderBarycentric: {
                        if (has_extension(VK_NV_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME)) {
                            barycentric_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV;
                            barycentric_features.fragmentShaderBarycentric = true;
                            insert_chain(&features2, &barycentric_features);
                            enabled_extensions.emplace_back(VK_NV_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
                        } else {
                            AM_LOG_ERROR(logger, "fragment shader barycentric extension is not supported");
                        }
                    } break;

                    default: AM_UNREACHABLE();
                }
            }
#if defined(AM_DEBUG)
            if (has_extension(VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
                enabled_extensions.emplace_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
                result->_features_custom.debug_names = true;
            }
            if (has_extension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME)) {
                enabled_extensions.emplace_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
            }
#endif
#if defined(AM_ENABLE_AFTERMATH)
            result->_aftermath_context = std::make_unique<CGPUCrashTrackerNV>();
            AM_ASSERT(GFSDK_Aftermath_EnableGpuCrashDumps(
                GFSDK_Aftermath_Version_API,
                GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
                GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default,
                [](const void* dump, const uint32 size, void* user) {
                    static_cast<CGPUCrashTrackerNV*>(user)->on_crash_dump(dump, size);
                },
                [](const void* dump, const uint32 size, void* user) {
                    static_cast<CGPUCrashTrackerNV*>(user)->on_shader_debug_info(dump, size);
                },
                [](PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription fn, void* user) {
                    static_cast<CGPUCrashTrackerNV*>(user)->on_description(fn);
                },
                [](const void*, void*, void**, uint32*) {},
                result->_aftermath_context.get()) == GFSDK_Aftermath_Result_Success, "Aftermath failure");
            VkDeviceDiagnosticsConfigCreateInfoNV aftermath_info = {};
            if (has_extension(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME)) {
                enabled_extensions.emplace_back(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);
                aftermath_info.sType = VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV;
                aftermath_info.flags =
                    VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV |
                    VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |
                    VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV |
                    VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV;
                insert_chain(&features2, &aftermath_info);
            }
#endif
            VkDeviceCreateInfo device_info = {};
            device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_info.pNext = &features2;
            device_info.pQueueCreateInfos = queue_infos.data();
            device_info.queueCreateInfoCount = (uint32)queue_infos.size();
            device_info.enabledExtensionCount = (uint32)enabled_extensions.size();
            device_info.ppEnabledExtensionNames = enabled_extensions.data();
            device_info.pEnabledFeatures = nullptr;
            AM_VULKAN_CHECK(logger, vkCreateDevice(result->_gpu, &device_info, nullptr, &result->_handle));
            volkLoadDevice(result->_handle);

            VkQueue handle;
            vkGetDeviceQueue(result->_handle, graphics_family.family, graphics_family.index, &handle);
            result->_graphics = CQueue::make(result, {
                .name = "graphics",
                .handle = handle,
                .family = graphics_family
            });
            result->_transfer = result->_graphics;
            result->_compute = result->_transfer;
            AM_LIKELY_IF(graphics_family.family != transfer_family.family) {
                vkGetDeviceQueue(result->_handle, transfer_family.family, transfer_family.index, &handle);
                result->_transfer = CQueue::make(result, {
                    .name = "transfer",
                    .handle = handle,
                    .family = transfer_family
                });
            }

            AM_LIKELY_IF(transfer_family.family != compute_family.family) {
                vkGetDeviceQueue(result->_handle, compute_family.family, compute_family.index, &handle);
                result->_compute = CQueue::make(result, {
                    .name = "compute",
                    .handle = handle,
                    .family = compute_family
                });
            }
        }
        { // Vulkan Memory Allocator initialization
            AM_LOG_INFO(logger, "initializing allocator");
            VmaVulkanFunctions vulkan_functions = {};
            vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
            vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

            VmaAllocatorCreateInfo allocator_info = {};
            allocator_info.flags = {};
            if (result->_features_12.bufferDeviceAddress) {
                allocator_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
            }
            allocator_info.physicalDevice = result->_gpu;
            allocator_info.device = result->_handle;
            allocator_info.pVulkanFunctions = &vulkan_functions;
            allocator_info.instance = context->native();
            allocator_info.vulkanApiVersion = api_version;
            AM_VULKAN_CHECK(logger, vmaCreateAllocator(&allocator_info, &result->_allocator));
        }
        { // CBufferSuballocator initialization
            result->_virtual_allocators.resize((uint32)EVirtualAllocatorKind::Count);
            result->_virtual_allocators[(uint32)EVirtualAllocatorKind::VertexBuffer] =
                CVirtualAllocator::make(result, EBufferUsage::VertexBuffer | EBufferUsage::TransferDST);
            result->_virtual_allocators[(uint32)EVirtualAllocatorKind::IndexBuffer] =
                CVirtualAllocator::make(result, EBufferUsage::IndexBuffer | EBufferUsage::TransferDST);
            result->_virtual_allocators[(uint32)EVirtualAllocatorKind::StagingBuffer] =
                CVirtualAllocator::make(result, EBufferUsage::TransferSRC, true);
        }
        result->_logger = std::move(logger);
        result->_context = std::move(context);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD VkDevice CDevice::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD VkPhysicalDevice CDevice::gpu() const noexcept {
        AM_PROFILE_SCOPED();
        return _gpu;
    }

    AM_NODISCARD VmaAllocator CDevice::allocator() const noexcept {
        AM_PROFILE_SCOPED();
        return _allocator;
    }

    AM_NODISCARD CQueue* CDevice::graphics_queue() const noexcept {
        AM_PROFILE_SCOPED();
        return _graphics;
    }

    AM_NODISCARD CQueue* CDevice::transfer_queue() const noexcept {
        AM_PROFILE_SCOPED();
        return _transfer;
    }

    AM_NODISCARD CQueue* CDevice::compute_queue() const noexcept {
        AM_PROFILE_SCOPED();
        return _compute;
    }

    AM_NODISCARD CContext* CDevice::context() noexcept {
        AM_PROFILE_SCOPED();
        return _context.get();
    }

    AM_NODISCARD const CContext* CDevice::context() const noexcept {
        AM_PROFILE_SCOPED();
        return _context.get();
    }

    AM_NODISCARD spdlog::logger* CDevice::logger() const noexcept {
        AM_PROFILE_SCOPED();
        return _logger.get();
    }

    const VkPhysicalDeviceProperties& CDevice::properties() const noexcept {
        AM_PROFILE_SCOPED();
        return _properties.properties;
    }

    AM_NODISCARD const VkPhysicalDeviceLimits& CDevice::limits() const noexcept {
        AM_PROFILE_SCOPED();
        return _properties.properties.limits;
    }

    AM_NODISCARD const uint8 (&CDevice::uuid() const noexcept)[VK_UUID_SIZE] {
        AM_PROFILE_SCOPED();
        return _gpu_id.deviceUUID;
    }

    AM_NODISCARD CVirtualAllocator* CDevice::virtual_allocator(EVirtualAllocatorKind kind) noexcept {
        AM_PROFILE_SCOPED();
        return _virtual_allocators[(uint32)kind].get();
    }

    AM_NODISCARD uint32 CDevice::memory_type_index(uint32 filter, EMemoryProperty flags) noexcept {
        AM_PROFILE_SCOPED();
        const auto v_flags = prv::as_vulkan(flags);
        for (uint32 i = 0; i < _memory_props.memoryTypeCount; ++i) {
            if ((filter & (1 << i)) && (_memory_props.memoryTypes[i].propertyFlags & v_flags) == v_flags) {
                return i;
            }
        }
        return (uint32)-1;
    }

    AM_NODISCARD bool CDevice::feature_support(EDeviceFeature feature) const noexcept {
        AM_PROFILE_SCOPED();
        switch (feature) {
            case EDeviceFeature::DebugNames:
                return _features_custom.debug_names;

            case EDeviceFeature::BufferDeviceAddress:
                 return _features_12.bufferDeviceAddress;

            default: AM_UNREACHABLE();
        }
        AM_UNREACHABLE();
    }

    AM_NODISCARD std::vector<SHeapBudget> CDevice::heap_budgets() const noexcept {
        AM_PROFILE_SCOPED();
        std::vector<VmaBudget> budgets(_memory_props.memoryHeapCount);
        vmaGetHeapBudgets(_allocator, budgets.data());
        std::vector<SHeapBudget> result;
        result.reserve(_memory_props.memoryHeapCount);
        for (auto& budget : budgets) {
            result.push_back({
                budget.statistics.blockCount,
                budget.statistics.allocationCount,
                budget.statistics.blockBytes,
                budget.statistics.allocationBytes
            });
        }
        return result;
    }

    AM_NODISCARD uint32 CDevice::acquire_image(CSwapchain* swapchain, const CSemaphore* semaphore) const noexcept {
        AM_PROFILE_SCOPED();
        uint32 index;
        auto result = vkAcquireNextImageKHR(_handle, swapchain->native(), (uint64)-1, semaphore->native(), nullptr, &index);
        AM_UNLIKELY_IF(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_SURFACE_LOST_KHR) {
            swapchain->set_lost();
            result = VK_SUCCESS;
        }
        AM_VULKAN_CHECK(_logger, result);
        return index;
    }

    AM_NODISCARD STextureInfo CDevice::sample(const CAsyncTexture* texture, SSamplerInfo info, bool reduction) noexcept {
        AM_PROFILE_SCOPED();
        return sample(texture->handle(), info, reduction);
    }

    AM_NODISCARD STextureInfo CDevice::sample(const CImage* image, SSamplerInfo info, bool reduction) noexcept {
        AM_PROFILE_SCOPED();
        auto sampler = _make_sampler(info, reduction);
        return {
            image->view(),
            sampler,
            prv::as_vulkan(info.layout)
        };
    }

    AM_NODISCARD STextureInfo CDevice::sample(const CImageView* view, SSamplerInfo info, bool reduction) noexcept {
        AM_PROFILE_SCOPED();
        auto sampler = _make_sampler(info, reduction);
        return {
            view->native(),
            sampler,
            prv::as_vulkan(info.layout)
        };
    }

    AM_NODISCARD VkDescriptorSetLayout CDevice::acquire_cached_item(const std::vector<SDescriptorBinding>& bindings) noexcept {
        AM_PROFILE_SCOPED();
        return _set_layout_cache[prv::hash(0, bindings)];
    }

    void CDevice::set_cached_item(const std::vector<SDescriptorBinding>& bindings, VkDescriptorSetLayout layout) noexcept {
        AM_PROFILE_SCOPED();
        _set_layout_cache[prv::hash(0, bindings)] = layout;
    }

    AM_NODISCARD VkSampler CDevice::acquire_cached_item(const SSamplerInfo& info) noexcept {
        AM_PROFILE_SCOPED();
        return _sampler_cache[prv::hash(0, info)];
    }

    void CDevice::set_cached_item(const SSamplerInfo& info, VkSampler sampler) noexcept {
        AM_PROFILE_SCOPED();
        _sampler_cache[prv::hash(0, info)] = sampler;
    }

    void CDevice::cleanup_after(uint32 frames, std::function<void(const Self*)>&& func) noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(_logger, "cleanup payload enqueued, after: {} frames", frames);
        _to_delete.push_back({
            ._frames = frames,
            ._elapsed = 0,
            ._func = std::move(func)
        });
    }

    void CDevice::update_cleanup() noexcept {
        AM_PROFILE_SCOPED();
        AM_LIKELY_IF(_to_delete.empty()) {
            return;
        }
        auto* current = &_to_delete.front();
        while (current) {
            AM_UNLIKELY_IF(current->_elapsed == current->_frames) {
                AM_LOG_INFO(_logger, "cleaning up payload: {}", (const void*)current);
                current->_func(this);
                _to_delete.pop_front();
                AM_UNLIKELY_IF(!_to_delete.empty()) {
                    current = &_to_delete.front();
                } else {
                    current = nullptr;
                }
            } else {
                current = nullptr;
            }
        }

        for (auto& each : _to_delete) {
            each._elapsed++;
        }
    }

    void CDevice::wait_idle() const noexcept {
        AM_PROFILE_SCOPED();
        _graphics->wait_idle();
        _transfer->wait_idle();
        _compute->wait_idle();
        AM_VULKAN_CHECK(_logger, vkDeviceWaitIdle(_handle));
    }

    AM_NODISCARD VkSampler CDevice::_make_sampler(SSamplerInfo info, bool reduction) noexcept {
        AM_PROFILE_SCOPED();
        auto sampler = acquire_cached_item(info);
        if (!sampler) {
            VkSamplerCreateInfo sampler_info = {};
            sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_info.magFilter = prv::as_vulkan(info.filter);
            sampler_info.minFilter = prv::as_vulkan(info.filter);
            sampler_info.mipmapMode = prv::as_vulkan(info.mip_mode);
            sampler_info.addressModeU = prv::as_vulkan(info.address_mode);
            sampler_info.addressModeV = prv::as_vulkan(info.address_mode);
            sampler_info.addressModeW = prv::as_vulkan(info.address_mode);
            sampler_info.mipLodBias = 0.0f;
            sampler_info.anisotropyEnable = std::fpclassify(info.anisotropy) != FP_ZERO;
            sampler_info.maxAnisotropy = info.anisotropy;
            sampler_info.compareEnable = false;
            sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
            sampler_info.minLod = 0.0f;
            sampler_info.maxLod = 16.0f;
            sampler_info.borderColor = prv::as_vulkan(info.border_color);
            sampler_info.unnormalizedCoordinates = false;
            VkSamplerReductionModeCreateInfo reduction_mode_info = {};
            if (reduction) {
                reduction_mode_info.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
                reduction_mode_info.reductionMode = prv::as_vulkan(info.reduction_mode);
                sampler_info.pNext = &reduction_mode_info;
            }
            AM_VULKAN_CHECK(_logger, vkCreateSampler(_handle, &sampler_info, nullptr, &sampler));
            set_cached_item(info, sampler);
        }
        return sampler;
    }
} // namespace am
