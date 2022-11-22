#include <amethyst/window/windowing_system.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace am {
    CWindowingSystem::CWindowingSystem() noexcept = default;

    CWindowingSystem::~CWindowingSystem() noexcept {
        AM_PROFILE_SCOPED();
        glfwTerminate();
    }

    AM_NODISCARD std::unique_ptr<CWindowingSystem> CWindowingSystem::make() noexcept {
        AM_PROFILE_SCOPED();
        AM_ASSERT(glfwInit(), "windowing system could not initialize");
        auto* result = new Self();
        return std::unique_ptr<Self>(result);
    }

    AM_NODISCARD std::vector<const char*> CWindowingSystem::surface_extensions() const noexcept {
        AM_PROFILE_SCOPED();
        uint32 count;
        auto extensions = glfwGetRequiredInstanceExtensions(&count);
        return { extensions, extensions + count };
    }

    AM_NODISCARD float64 CWindowingSystem::current_time() const noexcept {
        AM_PROFILE_SCOPED();
        return glfwGetTime();
    }

    void CWindowingSystem::poll_events() const noexcept {
        AM_PROFILE_SCOPED();
        glfwPollEvents();
    }

    void CWindowingSystem::wait_events() const noexcept {
        AM_PROFILE_SCOPED();
        glfwWaitEvents();
    }
} // namespace am
