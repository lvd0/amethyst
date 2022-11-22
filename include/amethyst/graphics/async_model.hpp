#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/async_texture.hpp>
#include <amethyst/graphics/async_mesh.hpp>
#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/fence.hpp>

#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#include <glm/mat4x4.hpp>

#include <filesystem>
#include <vector>

namespace am {
    struct SAABB {
        glm::vec3 center = {};
        glm::vec3 extents = {};
        glm::vec3 min = {};
        glm::vec3 max = {};
    };

    struct SMaterial {
        glm::vec4 base_color = {};
    };

    struct STexturedMesh {
        CRcPtr<CAsyncMesh> geometry;
        CRcPtr<CAsyncTexture> albedo;
        CRcPtr<CAsyncTexture> normal;
        SMaterial material = {};
        uint32 vertices = 0;
        uint32 indices = 0;
        glm::mat4 transform = {};
        SAABB aabb = {};
    };

    class AM_MODULE CAsyncModel : public IRefCounted {
    public:
        using Self = CAsyncModel;

        ~CAsyncModel() noexcept;

        AM_NODISCARD static CRcPtr<Self> sync_make(CRcPtr<CDevice>, std::filesystem::path&&) noexcept;
        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, std::filesystem::path&&) noexcept;

        AM_NODISCARD const std::vector<STexturedMesh>& submeshes() const noexcept;

        AM_NODISCARD bool is_ready() const noexcept;
        void wait() const noexcept;

    private:
        CAsyncModel() noexcept;

        std::vector<STexturedMesh> _submeshes;

        mutable std::unique_ptr<enki::TaskSet> _task; // nullptr if it was not requested via "make()"

        CRcPtr<CDevice> _device;
    };
} // namespace am
