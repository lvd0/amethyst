#pragma once

#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/graphics/descriptor_pool.hpp>
#include <amethyst/graphics/pipeline.hpp>
#include <amethyst/graphics/device.hpp>

#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

#include <vector>

namespace am {
    class AM_MODULE CDescriptorSet : public IRefCounted {
    public:
        using Self = CDescriptorSet;
        struct SCreateInfo {
            CRcPtr<CDescriptorPool> pool;
            CRcPtr<CPipeline> pipeline;
            uint32 index = 0;
        };
        struct SRawCreateInfo {
            CRcPtr<CDescriptorPool> pool;
            VkDescriptorSetLayout layout;
            uint32 index = 0;
            uint32 dynamic_count = 0;
        };

        ~CDescriptorSet() noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, const SCreateInfo&) noexcept;
        AM_NODISCARD static std::vector<CRcPtr<Self>> make(const CRcPtr<CDevice>&, uint32, SCreateInfo&&) noexcept;

        AM_NODISCARD static CRcPtr<Self> make(CRcPtr<CDevice>, const SRawCreateInfo&) noexcept;
        AM_NODISCARD static std::vector<CRcPtr<Self>> make(const CRcPtr<CDevice>&, uint32, SRawCreateInfo&&) noexcept;

        AM_NODISCARD VkDescriptorSet native() const noexcept;
        AM_NODISCARD uint32 index() const noexcept;

        Self& bind(const std::string&, SBufferInfo, uint32 = 0) noexcept;
        Self& bind(const std::string&, const std::vector<SBufferInfo>&, uint32 = 0) noexcept;
        Self& bind(const std::string&, const CImage*, uint32 = 0) noexcept;
        Self& bind(const std::string&, const CImageView*, uint32 = 0) noexcept;
        Self& bind(const std::string&, STextureInfo, uint32 = 0) noexcept;
        Self& bind(const std::string&, const std::vector<CRcPtr<CImageView>>&, uint32 = 0) noexcept;
        Self& bind(const std::string&, const std::vector<STextureInfo>&, uint32 = 0) noexcept;

        Self& bind(const SDescriptorBinding&, STextureInfo, uint32 = 0) noexcept;

        void update_pipeline(CRcPtr<CPipeline>) noexcept;

    private:
        struct SCachedDescriptors {
            uint64 binding_hash = 0;
            uint64 descriptor_hash = 0;
        };

        CDescriptorSet() noexcept;

        VkDescriptorSet _handle = {};
        VkDescriptorPool _native_pool = {};
        uint32 _index = 0;
        std::vector<SCachedDescriptors> _cache;

        CRcPtr<CDescriptorPool> _pool;
        CRcPtr<CPipeline> _pipeline;
        CRcPtr<CDevice> _device;
    };

    AM_NODISCARD AM_MODULE SDescriptorBinding make_descriptor_binding(uint32, EDescriptorType) noexcept;
} // namespace am
