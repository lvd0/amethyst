#include <amethyst/graphics/context.hpp>

#include <TaskScheduler.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace am {
    CContext::CContext() noexcept = default;

    CContext::~CContext() noexcept {
        AM_PROFILE_SCOPED();
#if defined(AM_DEBUG)
        vkDestroyDebugUtilsMessengerEXT(_handle, _validation, nullptr);
#endif
        AM_LOG_INFO(_logger, "terminating context");
        vkDestroyInstance(_handle, nullptr);
    }

    AM_NODISCARD CRcPtr<CContext> CContext::make(SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        auto logger = spdlog::stdout_color_mt("context");
        result->_logger = logger;
        { // Instance
            AM_VULKAN_CHECK(logger, volkInitialize());
            std::vector<const char*> extensions = std::move(info.surface_extensions);
#if defined(AM_DEBUG)
            extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            constexpr auto extra_validation = std::to_array({
                VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
                VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
            });
#endif
            uint32 version;
            AM_VULKAN_CHECK(logger, vkEnumerateInstanceVersion(&version));
            if (version <= VK_API_VERSION_1_1) {
                logger->critical("API version too old");
                AM_ASSERT(false, "fatal error");
            }

            AM_LOG_INFO(logger, "initializing core context");
            VkApplicationInfo application_info = {};
            application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            application_info.pApplicationName = "Amethyst Application";
            application_info.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
            application_info.pEngineName = "Amethyst Engine";
            application_info.engineVersion = version;
            application_info.apiVersion = version;

            VkInstanceCreateInfo instance_info = {};
            instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instance_info.pApplicationInfo = &application_info;
#if defined(AM_DEBUG)
            const char* validation_layer = "VK_LAYER_KHRONOS_validation";
            instance_info.enabledLayerCount = 1;
            instance_info.ppEnabledLayerNames = &validation_layer;

            VkValidationFeaturesEXT validation_features = {};
            validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
            validation_features.enabledValidationFeatureCount = (uint32)extra_validation.size();
            validation_features.pEnabledValidationFeatures = extra_validation.data();
            if (std::any_of(info.main_extensions.begin(), info.main_extensions.end(), [](const auto ext) {
                return ext == EAPIExtension::ExtraValidation;
            })) {
                AM_LOG_INFO(logger, "extra validation features enabled");
                instance_info.pNext = &validation_features;
            }
#endif
            instance_info.enabledExtensionCount = (uint32)extensions.size();
            instance_info.ppEnabledExtensionNames = extensions.data();
            AM_VULKAN_CHECK(logger, vkCreateInstance(&instance_info, nullptr, &result->_handle));
            volkLoadInstance(result->_handle);
        }
#if defined(AM_DEBUG)
        { // Validation
            VkDebugUtilsMessengerCreateInfoEXT validation_info;
            validation_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            validation_info.pNext = nullptr;
            validation_info.flags = {};
            validation_info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            validation_info.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            validation_info.pfnUserCallback = +[](VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                  VkDebugUtilsMessageTypeFlagsEXT type,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                  void* user_data) -> VkBool32 {
                auto logger = static_cast<spdlog::logger*>(user_data);
                spdlog::level::level_enum level;
                switch (severity) {
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                        level = spdlog::level::debug;
                        break;
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                        level = spdlog::level::info;
                        break;
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                        level = spdlog::level::warn;
                        break;
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                        level = spdlog::level::err;
                        break;
                    default: AM_UNREACHABLE();
                }

                const char* type_string = nullptr;
                switch (type) {
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
                        type_string = "general";
                        break;
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
                        type_string = "validation";
                        break;
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
                        type_string = "performance";
                        break;
                    default: AM_UNREACHABLE();
                }

    #if defined(AM_DEBUG) || defined(AM_DEBUG_LOGGING)
                logger->log(level, "[{}]: {}", type_string, data->pMessage);
                logger->flush();
    #endif
                const auto fatal_bits = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                AM_UNLIKELY_IF(severity & fatal_bits) {
                    AM_ASSERT(false, "fatal vulkan error");
                }
                return 0;
            };
            validation_info.pUserData = logger.get();
            AM_VULKAN_CHECK(logger, vkCreateDebugUtilsMessengerEXT(result->_handle, &validation_info, nullptr, &result->_validation));
        }
#endif
        { // Task Scheduler
            result->_scheduler = std::make_unique<enki::TaskScheduler>();
            result->_scheduler->Initialize();
        }
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD VkInstance CContext::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD enki::TaskScheduler* CContext::scheduler() noexcept {
        AM_PROFILE_SCOPED();
        return _scheduler.get();
    }
} // namespace am
