#pragma once

#include <amethyst/core/ref_counted.hpp>
#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#if defined(_WIN32)
    #pragma warning(push, 0)
#endif

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
    #pragma warning(pop)
#endif

#include <string>

namespace am {
    class AM_MODULE CWindow : public IRefCounted {
    public:
        using Self = CWindow;
        struct SCreateInfo {
            uint32 width = 0;
            uint32 height = 0;
            const char* title = nullptr;
        };

        ~CWindow() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(SCreateInfo&&) noexcept;

        AM_NODISCARD GLFWwindow* native() const noexcept;
        AM_NODISCARD uint32 width() const noexcept;
        AM_NODISCARD uint32 height() const noexcept;
        AM_NODISCARD const char* title() const noexcept;

        AM_NODISCARD bool is_open() const noexcept;
        void update_viewport() noexcept;
        void close() noexcept;

    private:
        CWindow() noexcept;

        GLFWwindow* _handle = nullptr;
        uint32 _width = 0;
        uint32 _height = 0;
        std::string _title = {};

        std::shared_ptr<spdlog::logger> _logger;
    };
} // namespace am
