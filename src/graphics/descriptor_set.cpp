#include <amethyst/graphics/descriptor_set.hpp>
#include <amethyst/graphics/async_texture.hpp>
#include <amethyst/graphics/typed_buffer.hpp>
#include <amethyst/graphics/pipeline.hpp>

#include <amethyst/meta/hash.hpp>

#include <algorithm>

namespace am {
    CDescriptorSet::CDescriptorSet() noexcept = default;

    CDescriptorSet::~CDescriptorSet() noexcept {
        AM_PROFILE_SCOPED();
        vkFreeDescriptorSets(_device->native(), _native_pool, 1, &_handle);
    }

    AM_NODISCARD CRcPtr<CDescriptorSet> CDescriptorSet::make(CRcPtr<CDevice> device, SCreateInfo info) noexcept {
        AM_PROFILE_SCOPED();
        auto result = new Self();
        const auto layout = info.pipeline->set_layout(info.layout);
        VkDescriptorSetAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.descriptorPool = info.pool->fetch_pool();
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &layout.handle;

        VkDescriptorSetVariableDescriptorCountAllocateInfo variable_count_info = {};
        if (layout.dynamic) {
            variable_count_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
            variable_count_info.descriptorSetCount = 1;
            variable_count_info.pDescriptorCounts = &layout.binds;
            allocate_info.pNext = &variable_count_info;
        }

        AM_VULKAN_CHECK(device->logger(), vkAllocateDescriptorSets(device->native(), &allocate_info, &result->_handle));
        result->_cache.reserve(1024);
        result->_index = info.layout;
        result->_native_pool = allocate_info.descriptorPool;
        result->_pipeline = std::move(info.pipeline);
        result->_pool = std::move(info.pool);
        result->_device = std::move(device);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD std::vector<CRcPtr<CDescriptorSet>> CDescriptorSet::make(const CRcPtr<CDevice>& device, uint32 count, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        std::vector<CRcPtr<Self>> result;
        result.reserve(count);
        for (uint32 i = 0; i < count; ++i) {
            result.emplace_back(make(device, info));
        }
        return result;
    }

    AM_NODISCARD VkDescriptorSet CDescriptorSet::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD uint32 CDescriptorSet::index() const noexcept {
        AM_PROFILE_SCOPED();
        return _index;
    }

    CDescriptorSet& CDescriptorSet::bind(const std::string& name, STypedBufferInfo info) noexcept {
        AM_PROFILE_SCOPED();
        const auto binding = _pipeline->bindings(name);
        const auto binding_hash = prv::hash(0, binding);
        const auto descriptor_hash = prv::hash(0, info);
        const auto is_bound = std::find_if(_cache.begin(), _cache.end(), [=](const auto& each) {
            return each.binding_hash == binding_hash;
        });
        const auto found_binding = is_bound != _cache.end();
        AM_UNLIKELY_IF(!found_binding || is_bound->descriptor_hash != descriptor_hash) {
            AM_LOG_WARN(_device->logger(), "updating descriptor: buffer: {}, binding: {}, range: {}",
                        (const void*)info.handle, binding.index, info.size);
            VkDescriptorBufferInfo descriptor = {};
            descriptor.buffer = info.handle;
            descriptor.offset = info.offset;
            descriptor.range = info.size;
            VkWriteDescriptorSet update = {};
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.dstSet = _handle;
            update.dstBinding = binding.index;
            update.dstArrayElement = 0;
            update.descriptorCount = 1;
            update.descriptorType = binding.type;
            update.pBufferInfo = &descriptor;
            vkUpdateDescriptorSets(_device->native(), 1, &update, 0, nullptr);
            if (!found_binding) {
                _cache.push_back({ binding_hash, descriptor_hash });
            } else {
                is_bound->descriptor_hash = descriptor_hash;
            }
        }
        return *this;
    }

    CDescriptorSet& CDescriptorSet::bind(const std::string& name, const CImage* image) noexcept {
        AM_PROFILE_SCOPED();
        const auto binding = _pipeline->bindings(name);
        const auto binding_hash = prv::hash(0, binding);
        const auto descriptor_hash = prv::hash(0, image);
        const auto is_bound = std::find_if(_cache.begin(), _cache.end(), [=](const auto& each) {
            return each.binding_hash == binding_hash;
        });
        const auto found_binding = is_bound != _cache.end();
        AM_UNLIKELY_IF(!found_binding || is_bound->descriptor_hash != descriptor_hash) {
            AM_LOG_WARN(_device->logger(), "updating descriptor: image: {}", (const void*)image->native());
            VkDescriptorImageInfo descriptor = {};
            descriptor.sampler = nullptr;
            descriptor.imageView = image->view();
            descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            VkWriteDescriptorSet update = {};
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.dstSet = _handle;
            update.dstBinding = binding.index;
            update.dstArrayElement = 0;
            update.descriptorCount = 1;
            update.descriptorType = binding.type;
            update.pImageInfo = &descriptor;
            vkUpdateDescriptorSets(_device->native(), 1, &update, 0, nullptr);
            if (!found_binding) {
                _cache.push_back({ binding_hash, descriptor_hash });
            } else {
                is_bound->descriptor_hash = descriptor_hash;
            }
        }
        return *this;
    }

    CDescriptorSet& CDescriptorSet::bind(const std::string& name, STextureInfo texture) noexcept {
        AM_PROFILE_SCOPED();
        const auto binding = _pipeline->bindings(name);
        const auto binding_hash = prv::hash(0, binding);
        const auto descriptor_hash = prv::hash(0, texture);
        const auto is_bound = std::find_if(_cache.begin(), _cache.end(), [=](const auto& each) {
            return each.binding_hash == binding_hash;
        });
        const auto found_binding = is_bound != _cache.end();
        AM_UNLIKELY_IF(!found_binding || is_bound->descriptor_hash != descriptor_hash) {
            AM_LOG_WARN(_device->logger(), "updating descriptor: texture: {}, sampler: {}",
                        (const void*)texture.handle, (const void*)texture.sampler);
            VkDescriptorImageInfo descriptor = {};
            descriptor.sampler = texture.sampler;
            descriptor.imageView = texture.handle;
            descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkWriteDescriptorSet update = {};
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.dstSet = _handle;
            update.dstBinding = binding.index;
            update.dstArrayElement = 0;
            update.descriptorCount = 1;
            update.descriptorType = binding.type;
            update.pImageInfo = &descriptor;
            vkUpdateDescriptorSets(_device->native(), 1, &update, 0, nullptr);
            if (!found_binding) {
                _cache.push_back({ binding_hash, descriptor_hash });
            } else {
                is_bound->descriptor_hash = descriptor_hash;
            }
        }
        return *this;
    }

    CDescriptorSet& CDescriptorSet::bind(const std::string& name, const std::vector<STextureInfo>& textures) noexcept {
        AM_PROFILE_SCOPED();
        const auto binding = _pipeline->bindings(name);
        const auto binding_hash = prv::hash(0, binding);
        const auto descriptor_hash = prv::hash(0, textures);
        const auto is_bound = std::find_if(_cache.begin(), _cache.end(), [=](const auto& each) {
            return each.binding_hash == binding_hash;
        });
        const auto found_binding = is_bound != _cache.end();
        AM_UNLIKELY_IF(!found_binding || is_bound->descriptor_hash != descriptor_hash) {
            AM_LOG_WARN(_device->logger(), "updating dynamic descriptor: size: {}", textures.size());
            std::vector<VkDescriptorImageInfo> descriptors;
            descriptors.reserve(descriptors.size());
            for (auto [handle, sampler] : textures) {
                VkDescriptorImageInfo descriptor = {};
                descriptor.sampler = sampler;
                descriptor.imageView = handle;
                descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                descriptors.emplace_back(descriptor);
            }
            VkWriteDescriptorSet update = {};
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.dstSet = _handle;
            update.dstBinding = binding.index;
            update.dstArrayElement = 0;
            update.descriptorCount = (uint32)descriptors.size();
            update.descriptorType = binding.type;
            update.pImageInfo = descriptors.data();
            vkUpdateDescriptorSets(_device->native(), 1, &update, 0, nullptr);
            if (!found_binding) {
                _cache.push_back({ binding_hash, descriptor_hash });
            } else {
                is_bound->descriptor_hash = descriptor_hash;
            }
        }
        return *this;
    }
} // namespace am
