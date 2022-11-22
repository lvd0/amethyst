#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/meta/constants.hpp>
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

#include <functional>
#include <memory>

namespace am {
    class AM_MODULE CUIContext {
    public:
        using Self = CUIContext;
        struct SCreateInfo {
            CRcPtr<CDevice> device;
            CRcPtr<CWindow> window;
            CRcPtr<CSwapchain> swapchain;
            bool load = true;
        };

        ~CUIContext() noexcept;

        AM_NODISCARD static std::unique_ptr<Self> make(SCreateInfo&&) noexcept;

        void bind_framebuffer(const CImage*) noexcept;
        void resize_framebuffer(uint32, uint32) noexcept;
        void acquire_frame() noexcept;
        void render_frame(CCommandBuffer&, std::function<void()>&&) noexcept;
        AM_NODISCARD const CFramebuffer* framebuffer() const noexcept;
        AM_NODISCARD VkPipelineLayout pipeline_layout() noexcept;
        AM_NODISCARD VkDescriptorSetLayout set_layout() noexcept;

    private:
        CUIContext() noexcept;

        CRcPtr<CDevice> _device;
        CRcPtr<CSwapchain> _swapchain;
        CRcPtr<CDescriptorPool> _pool;
        CRcPtr<CRenderPass> _pass;
        CRcPtr<CFramebuffer> _framebuffer;

        std::shared_ptr<spdlog::logger> _logger;
    };
} // namespace am