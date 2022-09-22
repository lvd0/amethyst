#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#if defined(_MSC_VER)
    #pragma warning(push, 0)
#endif

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

#include <vector>

namespace am {
    enum class EAPIExtension {
        ExtraValidation
    };

    class AM_MODULE CContext : public IRefCounted {
    public:
        using Self = CContext;
        struct SCreateInfo {
            std::vector<EAPIExtension> main_extensions;
            std::vector<const char*> surface_extensions;
        };

        ~CContext() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(SCreateInfo&&) noexcept;

        AM_NODISCARD VkInstance native() const noexcept;
        AM_NODISCARD enki::TaskScheduler* scheduler() noexcept;

    private:
        CContext() noexcept;

        VkInstance _handle = {};
        VkDebugUtilsMessengerEXT _validation = {};

        std::unique_ptr<enki::TaskScheduler> _scheduler;

        std::shared_ptr<spdlog::logger> _logger;
    };
} // namespace am
