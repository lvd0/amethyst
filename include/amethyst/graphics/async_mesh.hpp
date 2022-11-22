#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/virtual_allocator.hpp>
#include <amethyst/graphics/typed_buffer.hpp>
#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/fence.hpp>

#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <vector>

namespace am {
    namespace prv {
        constexpr auto vertex_components = 12;

        struct SVertex {
            glm::vec3 position;
            glm::vec3 normal;
            glm::vec2 uv;
            glm::vec4 tangent;
        };
    } // namespace am::prv

    class AM_MODULE CAsyncMesh : public IRefCounted {
    public:
        using Self = CAsyncMesh;
        struct SCreateInfo {
            std::vector<float32> geometry;
            std::vector<uint32> indices;
        };

        ~CAsyncMesh() noexcept;

        AM_NODISCARD static CRcPtr<Self> sync_make(CRcPtr<CDevice>, SCreateInfo&&) noexcept;
        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, SCreateInfo&&) noexcept;

        AM_NODISCARD const CBufferSlice* vertices() const noexcept;
        AM_NODISCARD const CBufferSlice* indices() const noexcept;
        AM_NODISCARD uint64 vertex_offset() const noexcept;
        AM_NODISCARD uint64 index_offset() const noexcept;

        AM_NODISCARD bool is_ready() const noexcept;
        void wait() const noexcept;

    private:
        CAsyncMesh() noexcept;

        CBufferSlice _vertices;
        CBufferSlice _indices;
        mutable std::unique_ptr<enki::TaskSet> _task; // nullptr if it was not requested via "make()"

        CRcPtr<CDevice> _device;
    };
} // namespace am
