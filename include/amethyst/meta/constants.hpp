#pragma once

#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

namespace am {
    constexpr struct SInvertedViewportTag {} inverted_viewport_tag;

    constexpr auto frames_in_flight = 3u;
    constexpr auto all_layers = static_cast<uint32>(-1);
    constexpr auto all_mips = static_cast<uint32>(-1);
    constexpr auto external_subpass = VK_SUBPASS_EXTERNAL;
#if _WIN64
    constexpr auto external_memory_handle_type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    constexpr auto external_memory_handle_type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

    AM_NODISCARD AM_CONSTEXPR uint64 align_size(uint64 size, uint64 alignment) noexcept {
        return size + (-size & (alignment - 1ull));
    }
} // namespace am
