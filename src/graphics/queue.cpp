#include <amethyst/graphics/command_buffer.hpp>
#include <amethyst/graphics/swapchain.hpp>
#include <amethyst/graphics/semaphore.hpp>
#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/queue.hpp>
#include <amethyst/graphics/fence.hpp>

namespace am {
    CQueue::CQueue() noexcept = default;

    CQueue::~CQueue() noexcept {
        AM_PROFILE_SCOPED();
        vkDestroyCommandPool(_device->native(), _pool, nullptr);
        for (const auto& pool : _transient) {
            vkDestroyCommandPool(_device->native(), pool->_handle, nullptr);
        }
        AM_LOG_INFO(_logger, "terminating queue");
    }

    AM_NODISCARD CQueue* CQueue::make(CDevice* device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        auto logger = spdlog::stdout_color_mt(info.name);
        result->_handle = info.handle;
        result->_family = info.family;
        result->_type = info.type;

        AM_LOG_INFO(logger, "initializing queue with family: {}", info.family.family);
        VkCommandPoolCreateInfo command_pool_info;
        command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_info.pNext = nullptr;
        command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_info.queueFamilyIndex = info.family.family;
        AM_VULKAN_CHECK(device->logger(), vkCreateCommandPool(device->native(), &command_pool_info, nullptr, &result->_pool));

        command_pool_info.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        const auto threads = std::thread::hardware_concurrency();
        result->_transient.reserve(threads);
        for (uint32 i = 0; i < threads; ++i) {
            auto& pool = result->_transient.emplace_back(std::make_unique<SThreadSafePool>());
            AM_VULKAN_CHECK(device->logger(), vkCreateCommandPool(device->native(), &command_pool_info, nullptr, &pool->_handle));
        }
        result->_logger = std::move(logger);
        result->_device = device;
        return result;
    }

    AM_NODISCARD uint32 CQueue::family() const noexcept {
        AM_PROFILE_SCOPED();
        return _family.family;
    }

    AM_NODISCARD VkQueue CQueue::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD VkCommandPool CQueue::main_pool() const noexcept {
        AM_PROFILE_SCOPED();
        return _pool;
    }

    AM_NODISCARD VkCommandPool CQueue::transient_pool(uint32 thread) const noexcept {
        AM_PROFILE_SCOPED();
        return _transient[thread]->_handle;
    }

    void CQueue::lock_pool(uint32 thread) const noexcept {
        AM_PROFILE_SCOPED();
        _transient[thread]->_lock.lock();
    }

    void CQueue::unlock_pool(uint32 thread) const noexcept {
        AM_PROFILE_SCOPED();
        _transient[thread]->_lock.unlock();
    }

    void CQueue::wait_idle() noexcept {
        AM_PROFILE_SCOPED();
        std::lock_guard guard(_lock);
        AM_VULKAN_CHECK(_device->logger(), vkQueueWaitIdle(_handle));
    }

    void CQueue::submit(std::vector<SQueueSubmitInfo>&& info, CFence* fence) noexcept {
        AM_PROFILE_SCOPED();
        std::vector<VkPipelineStageFlags> stage_masks;
        stage_masks.reserve(info.size());
        std::vector<VkCommandBuffer> commands;
        commands.reserve(info.size());
        std::vector<VkSemaphore> waits;
        waits.reserve(info.size());
        std::vector<VkSemaphore> signals;
        signals.reserve(info.size());
        for (auto&& [stage_mask, command, wait, signal] : info) {
            AM_LIKELY_IF(command) {
                commands.emplace_back(command->native());
            }
            AM_LIKELY_IF(wait) {
                waits.emplace_back(wait->native());
                stage_masks.emplace_back(prv::as_vulkan(stage_mask));
            }
            AM_LIKELY_IF(signal) {
                signals.emplace_back(signal->native());
            }
        }
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = (uint32)waits.size();
        submit_info.pWaitSemaphores = waits.data();
        submit_info.pWaitDstStageMask = stage_masks.data();
        submit_info.commandBufferCount = (uint32)commands.size();
        submit_info.pCommandBuffers = commands.data();
        submit_info.signalSemaphoreCount = (uint32)signals.size();
        submit_info.pSignalSemaphores = signals.data();
        VkFence n_fence = nullptr;
        AM_LIKELY_IF(fence) {
            n_fence = fence->native();
        }
        std::lock_guard guard(_lock);
        AM_VULKAN_CHECK(_device->logger(), vkQueueSubmit(_handle, 1, &submit_info, n_fence));
    }

    void CQueue::immediate_submit(std::function<void(CCommandBuffer&)>&& func) noexcept {
        AM_PROFILE_SCOPED();
        auto commands = CCommandBuffer::make(CRcPtr<CDevice>::make(_device), {
            .queue = _type,
            .pool = ECommandPoolType::Main
        });
        commands->begin();
        func(*commands);
        commands->end();
        auto fence = CFence::make(CRcPtr<CDevice>::make(_device), false);
        submit({ { .command = commands.get() } }, fence.get());
        fence->wait();
    }

    void CQueue::present(SQueuePresentInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        const auto wait = info.wait->native();
        const auto swapchain = info.swapchain->native();
        VkResult result;
        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &wait;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain;
        present_info.pImageIndices = &info.image;
        present_info.pResults = &result;
        std::lock_guard guard(_lock);
        vkQueuePresentKHR(_handle, &present_info);
        AM_UNLIKELY_IF(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_SURFACE_LOST_KHR) {
            info.swapchain->set_lost();
            result = VK_SUCCESS;
        }
        AM_VULKAN_CHECK(_logger, result);
    }
} // namespace am
