#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/render_pass.hpp>
#include <amethyst/graphics/device.hpp>

#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#include <filesystem>
#include <map>

namespace am {
    enum class EPipelineType {
        Graphics,
        Compute,
        RayTracing
    };

    enum class EVertexAttribute {
        Vec1 = sizeof(float32[1]),
        Vec2 = sizeof(float32[2]),
        Vec3 = sizeof(float32[3]),
        Vec4 = sizeof(float32[4]),
    };

    enum class EAttachmentBlend {
        Auto,
        Disabled
    };

    struct SDescriptorBinding {
        bool dynamic = false;
        uint32 index = 0;
        uint32 count = 0;
        VkDescriptorType type = {};
        VkShaderStageFlags stage = {};

        AM_NODISCARD constexpr bool operator ==(const SDescriptorBinding& rhs) const noexcept {
            return dynamic == rhs.dynamic &&
                   index == rhs.index &&
                   count == rhs.count &&
                   type == rhs.type &&
                   stage == rhs.stage;
        }
    };

    struct SDescriptorSetLayout {
        VkDescriptorSetLayout handle = {};
        uint32 binds = 0;
        bool dynamic = false;
    };

    class AM_MODULE CPipeline : public IRefCounted {
    public:
        using Self = CPipeline;
        struct SGraphicsCreateInfo {
            std::filesystem::path vertex;
            std::filesystem::path fragment;
            std::filesystem::path geometry;
            std::vector<EVertexAttribute> attributes;
            std::vector<EAttachmentBlend> attachments;
            std::vector<EDynamicState> states;
            ECullMode cull = {};
            bool depth_test = false;
            bool depth_write = false;
            uint32 subpass = 0;
            const CFramebuffer* framebuffer = nullptr;
        };
        struct SComputeCreateInfo {
            std::filesystem::path compute;
        };

        ~CPipeline() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, SGraphicsCreateInfo&&) noexcept;
        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, SComputeCreateInfo&&) noexcept;

        AM_NODISCARD VkPipeline native() const noexcept;
        AM_NODISCARD EPipelineType type() const noexcept;
        AM_NODISCARD VkPipelineLayout main_layout() const noexcept;
        AM_NODISCARD SDescriptorSetLayout set_layout(uint32) const noexcept;
        AM_NODISCARD const SDescriptorBinding* bindings(const std::string&) const noexcept;

    private:
        CPipeline() noexcept;

        VkPipeline _handle = {};
        struct {
            VkPipelineLayout _pipeline = {};
            std::vector<SDescriptorSetLayout> _set;
        } _layout = {};

        std::map<std::string, SDescriptorBinding> _bindings;
        EPipelineType _type = {};

        CRcPtr<CDevice> _device;
    };
} // namespace am
