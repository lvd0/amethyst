#include <amethyst/window/window.hpp>

#include <GLFW/glfw3.h>

#include <volk.h>

namespace am {
    CWindow::CWindow() noexcept = default;

    CWindow::~CWindow() noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(_logger, "terminating window: {}", _title);
        glfwDestroyWindow(_handle);
    }

    AM_NODISCARD CRcPtr<CWindow> CWindow::make(SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto window = new Self();
        auto logger = spdlog::stdout_color_mt("window");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window->_handle = glfwCreateWindow(info.width, info.height, info.title, nullptr, nullptr);
        window->_width = info.width;
        window->_height = info.height;
        window->_title = info.title;
        AM_ASSERT(window->_handle, "window could not be created");
        AM_LOG_INFO(logger, "window created with parameters:");
        AM_LOG_INFO(logger, "- width: {}", info.width);
        AM_LOG_INFO(logger, "- height: {}", info.height);
        AM_LOG_INFO(logger, "- title: {}", info.title);
        glfwSetWindowUserPointer(window->_handle, window);
        glfwSetFramebufferSizeCallback(window->_handle, [](GLFWwindow* handle, int width, int height) {
            auto window = static_cast<CWindow*>(glfwGetWindowUserPointer(handle));
            AM_LOG_WARN(window->_logger, "window viewport update:");
            AM_LOG_WARN(window->_logger, "- width: {}", width);
            AM_LOG_WARN(window->_logger, "- height: {}", height);
            window->_width = width;
            window->_height = height;
        });
        window->_logger = std::move(logger);
        return CRcPtr<Self>::make(window);
    }

    AM_NODISCARD GLFWwindow* CWindow::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD uint32 CWindow::width() const noexcept {
        AM_PROFILE_SCOPED();
        return _width;
    }

    AM_NODISCARD uint32 CWindow::height() const noexcept {
        AM_PROFILE_SCOPED();
        return _height;
    }

    AM_NODISCARD const char* CWindow::title() const noexcept {
        AM_PROFILE_SCOPED();
        return _title.c_str();
    }

    AM_NODISCARD bool CWindow::is_open() const noexcept {
        AM_PROFILE_SCOPED();
        return !glfwWindowShouldClose(_handle);
    }

    void CWindow::update_viewport() noexcept {
        int32 new_width = 0;
        int32 new_height = 0;
        glfwGetFramebufferSize(_handle, &new_width, &new_height);
        _width = (uint32)new_width;
        _height = (uint32)new_height;
    }
} // namespace am
