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

    AM_NODISCARD CRcPtr<CDescriptorSet> CDescriptorSet::make(CRcPtr<CDevice> device, const SCreateInfo& info) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        const auto layout = info.pipeline->set_layout(info.index);
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
        result->_index = info.index;
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

    AM_NODISCARD CRcPtr<CDescriptorSet> CDescriptorSet::make(CRcPtr<CDevice> device, const SRawCreateInfo& info) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        VkDescriptorSetAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.descriptorPool = info.pool->fetch_pool();
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &info.layout;

        VkDescriptorSetVariableDescriptorCountAllocateInfo variable_count_info = {};
        if (info.dynamic_count > 0) {
            variable_count_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
            variable_count_info.descriptorSetCount = 1;
            variable_count_info.pDescriptorCounts = &info.dynamic_count;
            allocate_info.pNext = &variable_count_info;
        }

        AM_VULKAN_CHECK(device->logger(), vkAllocateDescriptorSets(device->native(), &allocate_info, &result->_handle));
        result->_cache.reserve(1024);
        result->_index = info.index;
        result->_native_pool = allocate_info.descriptorPool;
        result->_pipeline = nullptr;
        result->_pool = std::move(info.pool);
        result->_device = std::move(device);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD std::vector<CRcPtr<CDescriptorSet>> CDescriptorSet::make(const CRcPtr<CDevice>& device, uint32 count, SRawCreateInfo&& info) noexcept {
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

    CDescriptorSet& CDescriptorSet::bind(const std::string& name, SBufferInfo info, uint32 offset) noexcept {
        AM_PROFILE_SCOPED();
        const auto* binding = _pipeline->bindings(name);
        AM_UNLIKELY_IF(!binding) {
            return *this;
        }
        const auto binding_hash = prv::hash(0, binding);
        const auto descriptor_hash = prv::hash(0, info);
        const auto is_bound = std::find_if(_cache.begin(), _cache.end(), [=](const auto& each) {
            return each.binding_hash == binding_hash;
        });
        const auto found_binding = is_bound != _cache.end();
        AM_UNLIKELY_IF(!found_binding || is_bound->descriptor_hash != descriptor_hash) {
            VkDescriptorBufferInfo descriptor = {};
            descriptor.buffer = info.handle;
            descriptor.offset = info.offset;
            descriptor.range = info.size;
            VkWriteDescriptorSet update = {};
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.dstSet = _handle;
            update.dstBinding = binding->index;
            update.dstArrayElement = offset;
            update.descriptorCount = 1;
            update.descriptorType = binding->type;
            update.pBufferInfo = &descriptor;
            vkUpdateDescriptorSets(_device->native(), 1, &update, 0, nullptr);
            AM_LIKELY_IF(!found_binding) {
                _cache.push_back({ binding_hash, descriptor_hash });
            } else {
                is_bound->descriptor_hash = descriptor_hash;
            }
        }
        return *this;
    }

    CDescriptorSet& CDescriptorSet::bind(const std::string& name, const std::vector<SBufferInfo>& buffers, uint32 offset) noexcept {
        AM_PROFILE_SCOPED();
        const auto* binding = _pipeline->bindings(name);
        AM_UNLIKELY_IF(!binding) {
            return *this;
        }
        const auto binding_hash = prv::hash(0, binding);
        const auto descriptor_hash = prv::hash(0, buffers);
        const auto is_bound = std::find_if(_cache.begin(), _cache.end(), [=](const auto& each) {
            return each.binding_hash == binding_hash;
        });
        const auto found_binding = is_bound != _cache.end();
        AM_UNLIKELY_IF(!found_binding || is_bound->descriptor_hash != descriptor_hash) {
            AM_LOG_WARN(_device->logger(), "updating dynamic buffer descriptor: size: {}", buffers.size());
            std::vector<VkDescriptorBufferInfo> descriptors;
            descriptors.reserve(descriptors.size());
            for (const auto& buffer : buffers) {
                VkDescriptorBufferInfo descriptor = {};
                descriptor.buffer = buffer.handle;
                descriptor.offset = buffer.offset;
                descriptor.range = buffer.size;
                descriptors.emplace_back(descriptor);
            }
            VkWriteDescriptorSet update = {};
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.dstSet = _handle;
            update.dstBinding = binding->index;
            update.dstArrayElement = offset;
            update.descriptorCount = (uint32)descriptors.size();
            update.descriptorType = binding->type;
            update.pBufferInfo = descriptors.data();
            vkUpdateDescriptorSets(_device->native(), 1, &update, 0, nullptr);
            AM_LIKELY_IF(!found_binding) {
                _cache.push_back({ binding_hash, descriptor_hash });
            } else {
                is_bound->descriptor_hash = descriptor_hash;
            }
        }
        return *this;
    }

    CDescriptorSet& CDescriptorSet::bind(const std::string& name, const CImage* image, uint32 offset) noexcept {
        AM_PROFILE_SCOPED();
        const auto* binding = _pipeline->bindings(name);
        AM_UNLIKELY_IF(!binding) {
            return *this;
        }
        const auto binding_hash = prv::hash(0, binding);
        const auto descriptor_hash = prv::hash(0, image);
        const auto is_bound = std::find_if(_cache.begin(), _cache.end(), [=](const auto& each) {
            return each.binding_hash == binding_hash;
        });
        const auto found_binding = is_bound != _cache.end();
        AM_UNLIKELY_IF(!found_binding || is_bound->descriptor_hash != descriptor_hash) {
            VkDescriptorImageInfo descriptor = {};
            descriptor.sampler = nullptr;
            descriptor.imageView = image->view();
            descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            VkWriteDescriptorSet update = {};
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.dstSet = _handle;
            update.dstBinding = binding->index;
            update.dstArrayElement = offset;
            update.descriptorCount = 1;
            update.descriptorType = binding->type;
            update.pImageInfo = &descriptor;
            vkUpdateDescriptorSets(_device->native(), 1, &update, 0, nullptr);
            AM_LIKELY_IF(!found_binding) {
                _cache.push_back({ binding_hash, descriptor_hash });
            } else {
                is_bound->descriptor_hash = descriptor_hash;
            }
        }
        return *this;
    }

    CDescriptorSet& CDescriptorSet::bind(const std::string& name, const CImageView* view, uint32 offset) noexcept {
        AM_PROFILE_SCOPED();
        const auto* binding = _pipeline->bindings(name);
        AM_UNLIKELY_IF(!binding) {
            return *this;
        }
        const auto binding_hash = prv::hash(0, binding);
        const auto descriptor_hash = prv::hash(0, view);
        const auto is_bound = std::find_if(_cache.begin(), _cache.end(), [=](const auto& each) {
            return each.binding_hash == binding_hash;
        });
        const auto found_binding = is_bound != _cache.end();
        AM_UNLIKELY_IF(!found_binding || is_bound->descriptor_hash != descriptor_hash) {
            VkDescriptorImageInfo descriptor = {};
            descriptor.sampler = nullptr;
            descriptor.imageView = view->native();
            descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            VkWriteDescriptorSet update = {};
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.dstSet = _handle;
            update.dstBinding = binding->index;
            update.dstArrayElement = offset;
            update.descriptorCount = 1;
            update.descriptorType = binding->type;
            update.pImageInfo = &descriptor;
            vkUpdateDescriptorSets(_device->native(), 1, &update, 0, nullptr);
            AM_LIKELY_IF(!found_binding) {
                _cache.push_back({ binding_hash, descriptor_hash });
            } else {
                is_bound->descriptor_hash = descriptor_hash;
            }
        }
        return *this;
    }

    CDescriptorSet& CDescriptorSet::bind(const std::string& name, STextureInfo texture, uint32 offset) noexcept {
        AM_PROFILE_SCOPED();
        const auto* binding = _pipeline->bindings(name);
        AM_UNLIKELY_IF(!binding) {
            return *this;
        }
        return bind(*binding, texture, offset);
    }

    CDescriptorSet& CDescriptorSet::bind(const std::string& name, const std::vector<CRcPtr<CImageView>>& views, uint32 offset) noexcept {
        AM_PROFILE_SCOPED();
        const auto* binding = _pipeline->bindings(name);
        AM_UNLIKELY_IF(!binding) {
            return *this;
        }
        const auto binding_hash = prv::hash(0, binding);
        const auto descriptor_hash = prv::hash(0, views);
        const auto is_bound = std::find_if(_cache.begin(), _cache.end(), [=](const auto& each) {
            return each.binding_hash == binding_hash;
        });
        const auto found_binding = is_bound != _cache.end();
        AM_UNLIKELY_IF(!found_binding || is_bound->descriptor_hash != descriptor_hash) {
            AM_LOG_WARN(_device->logger(), "updating dynamic image descriptor: size: {}", views.size());
            std::vector<VkDescriptorImageInfo> descriptors;
            descriptors.reserve(descriptors.size());
            for (auto& view : views) {
                VkDescriptorImageInfo descriptor = {};
                descriptor.sampler = nullptr;
                descriptor.imageView = view->native();
                descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                descriptors.emplace_back(descriptor);
            }
            VkWriteDescriptorSet update = {};
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.dstSet = _handle;
            update.dstBinding = binding->index;
            update.dstArrayElement = offset;
            update.descriptorCount = (uint32)descriptors.size();
            update.descriptorType = binding->type;
            update.pImageInfo = descriptors.data();
            vkUpdateDescriptorSets(_device->native(), 1, &update, 0, nullptr);
            AM_LIKELY_IF(!found_binding) {
                _cache.push_back({ binding_hash, descriptor_hash });
            } else {
                is_bound->descriptor_hash = descriptor_hash;
            }
        }
        return *this;
    }

    CDescriptorSet& CDescriptorSet::bind(const std::string& name, const std::vector<STextureInfo>& textures, uint32 offset) noexcept {
        AM_PROFILE_SCOPED();
        const auto* binding = _pipeline->bindings(name);
        AM_UNLIKELY_IF(!binding) {
            return *this;
        }
        const auto binding_hash = prv::hash(0, binding);
        const auto descriptor_hash = prv::hash(0, textures);
        const auto is_bound = std::find_if(_cache.begin(), _cache.end(), [=](const auto& each) {
            return each.binding_hash == binding_hash;
        });
        const auto found_binding = is_bound != _cache.end();
        AM_UNLIKELY_IF(!found_binding || is_bound->descriptor_hash != descriptor_hash) {
            AM_LOG_WARN(_device->logger(), "updating dynamic texture descriptor: size: {}", textures.size());
            std::vector<VkDescriptorImageInfo> descriptors;
            descriptors.reserve(descriptors.size());
            for (auto [handle, sampler, layout] : textures) {
                VkDescriptorImageInfo descriptor = {};
                descriptor.sampler = sampler;
                descriptor.imageView = handle;
                descriptor.imageLayout = layout;
                descriptors.emplace_back(descriptor);
            }
            VkWriteDescriptorSet update = {};
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.dstSet = _handle;
            update.dstBinding = binding->index;
            update.dstArrayElement = offset;
            update.descriptorCount = (uint32)descriptors.size();
            update.descriptorType = binding->type;
            update.pImageInfo = descriptors.data();
            vkUpdateDescriptorSets(_device->native(), 1, &update, 0, nullptr);
            AM_LIKELY_IF(!found_binding) {
                _cache.push_back({ binding_hash, descriptor_hash });
            } else {
                is_bound->descriptor_hash = descriptor_hash;
            }
        }
        return *this;
    }

    CDescriptorSet& CDescriptorSet::bind(const SDescriptorBinding& binding, STextureInfo texture, uint32 offset) noexcept {
        const auto binding_hash = prv::hash(0, binding);
        const auto descriptor_hash = prv::hash(0, texture);
        const auto is_bound = std::find_if(_cache.begin(), _cache.end(), [=](const auto& each) {
            return each.binding_hash == binding_hash;
        });
        const auto found_binding = is_bound != _cache.end();
        AM_UNLIKELY_IF(!found_binding || is_bound->descriptor_hash != descriptor_hash) {
            AM_LOG_WARN(_device->logger(), "updating texture descriptor: handle: {}", (const void*)texture.handle);
            VkDescriptorImageInfo descriptor = {};
            descriptor.sampler = texture.sampler;
            descriptor.imageView = texture.handle;
            descriptor.imageLayout = texture.layout;
            VkWriteDescriptorSet update = {};
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.dstSet = _handle;
            update.dstBinding = binding.index;
            update.dstArrayElement = offset;
            update.descriptorCount = 1;
            update.descriptorType = binding.type;
            update.pImageInfo = &descriptor;
            vkUpdateDescriptorSets(_device->native(), 1, &update, 0, nullptr);
            AM_LIKELY_IF(!found_binding) {
                _cache.push_back({ binding_hash, descriptor_hash });
            } else {
                is_bound->descriptor_hash = descriptor_hash;
            }
        }
        return *this;
    }

    void CDescriptorSet::update_pipeline(CRcPtr<CPipeline> pipeline) noexcept {
        AM_PROFILE_SCOPED();
        _pipeline = std::move(pipeline);
    }

    AM_NODISCARD AM_MODULE SDescriptorBinding make_descriptor_binding(uint32 index, EDescriptorType type) noexcept {
        AM_PROFILE_SCOPED();
        SDescriptorBinding binding = {};
        binding.index = index;
        binding.type = prv::as_vulkan(type);
        return binding;
    }
} // namespace am
