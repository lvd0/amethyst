#pragma once

#include <amethyst/core/expected.hpp>
#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/image.hpp>
#include <amethyst/graphics/fence.hpp>

#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#include <filesystem>
#include <vector>

namespace am {
    enum class ETextureType {
        Color,
        NonColor
    };

    struct SSamplerInfo {
        EFilter filter = {};
        EBorderColor border_color = {};
        EAddressMode address_mode = {};
        float32 anisotropy = 0;
    };

    struct STextureInfo {
        VkImageView handle = {};
        VkSampler sampler = {};
    };

    class AM_MODULE CAsyncTexture : public IRefCounted {
    public:
        using Self = CAsyncTexture;
        struct SCreateInfo {
            std::filesystem::path path;
            ETextureType type = {};
        };

        ~CAsyncTexture() noexcept;

        AM_NODISCARD static CRcPtr<Self> sync_make(CRcPtr<CDevice>, SCreateInfo&&) noexcept;
        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, SCreateInfo&&) noexcept;

        AM_NODISCARD const CImage* handle() const noexcept;

        AM_NODISCARD bool is_ready() const noexcept;
        void wait() const noexcept;

    private:
        CAsyncTexture() noexcept;

        CRcPtr<CImage> _handle;

        mutable std::unique_ptr<enki::TaskSet> _task; // nullptr if it was not requested via "make()"

        CRcPtr<CDevice> _device;
    };
} // namespace am
