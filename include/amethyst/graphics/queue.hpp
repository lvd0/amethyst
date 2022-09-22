#pragma once

#include <amethyst/graphics/semaphore.hpp>
#include <amethyst/graphics/fence.hpp>

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

#include <vector>
#include <mutex>

namespace am {
    struct SQueueFamily {
        uint32 family = 0;
        uint32 index = 0;
    };

    struct SQueueSubmitInfo {
        EPipelineStage stage_mask = {};
        CCommandBuffer* command = nullptr;
        CSemaphore* wait = nullptr;
        CSemaphore* signal = nullptr;
    };

    struct SQueuePresentInfo {
        uint32 image = 0;
        CSwapchain* swapchain = nullptr;
        CSemaphore* wait = nullptr;
    };

    enum class EQueueType {
        Graphics,
        Transfer,
        Compute
    };

    enum class ECommandPoolType {
        Main,
        Transient
    };

    class AM_MODULE CQueue {
    public:
        using Self = CQueue;
        struct SCreateInfo {
            const char* name = nullptr;
            VkQueue handle = {};
            SQueueFamily family = {};
            EQueueType type = {};
        };

        ~CQueue() noexcept;

        AM_NODISCARD static Self* make(CDevice*, SCreateInfo&&) noexcept;

        AM_NODISCARD VkQueue native() const noexcept;
        AM_NODISCARD uint32 family() const noexcept;
        AM_NODISCARD VkCommandPool main_pool() const noexcept;
        AM_NODISCARD VkCommandPool transient_pool(uint32) const noexcept;

        void lock_pool(uint32) const noexcept;
        void unlock_pool(uint32) const noexcept;

        void wait_idle() noexcept;
        void submit(std::vector<SQueueSubmitInfo>&&, CFence*) noexcept;
        void immediate_submit(std::function<void(CCommandBuffer&)>&&) noexcept;
        void present(SQueuePresentInfo&&) noexcept;

    private:
        struct SThreadSafePool {
            VkCommandPool _handle = {};
            std::mutex _lock;
        };

        CQueue() noexcept;

        VkQueue _handle = {};
        VkCommandPool _pool = {};
        std::vector<std::unique_ptr<SThreadSafePool>> _transient;
        SQueueFamily _family = {};
        EQueueType _type = {};
        std::mutex _lock;

        CDevice* _device = nullptr;
        std::shared_ptr<spdlog::logger> _logger;
    };
} // namespace am
