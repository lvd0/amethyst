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
} // namespace am
